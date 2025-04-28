/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileCopyrightText: Huawei Inc.
 * SPDX-FileCopyrightText: Politecnico Di Milano
 */

#include "engine.h"
#include "eddie.h"
#include <json/json.h>
#include <utility>
#include <iostream>
#include <fstream>
#include <cmath>
#include <unistd.h>

Engine::Engine() {
    json_link_by_rt = std::unordered_multimap<std::string, std::pair<Json::Value, Link>>();
    link_by_href = std::unordered_map<std::string, Link>();
    eddie_endpoint = nullptr;

    // load dictionary
    Json::CharReaderBuilder readerBuilder;
    readerBuilder["indentation"] = "";
    std::string errors;
    std::ifstream t(DESCRIPTORS_DIR + std::string("/json/ontology/dictionary.json"));
    Json::parseFromStream(readerBuilder, t, &dictionary, &errors);
}

Engine::~Engine() {
    json_link_by_rt.clear();
    link_by_href.clear();
}

void Engine::set_endpoint(EddieEndpoint *ep) {
    eddie_endpoint = ep;
}

method_t method_from_string(const std::string &input) {
    if (input == "GET") return GET;
    else if (input == "POST") return POST;
    else if (input == "PUT") return PUT;
    else return DELETE;
}
void printConstGraph(std::unordered_map<std::string, std::vector<std::string>> constrGraph)
{
    for (auto i : constrGraph)
    {
        LOG_DBG("name: %s",i.first.c_str());

        LOG_DBG("Next nodes:");

        for (auto j : i.second)
        {
            LOG_DBG("%s", j.c_str());
        }
        
    }
    
}

void printDFS(std::vector<NodeDFS> dfs)
{
    for (auto i : dfs)
    {
        LOG_DBG("name: %s",i.nameNode.c_str());

        LOG_DBG("parent: %s", i.parent.c_str());

        LOG_DBG("Childrens:");

        for (auto child : i.childrens)
        {
            LOG_DBG("%s",child.c_str());
        }

        LOG_DBG("Pseudoparents:");

        for (auto pParent : i.pseudoParents)
        {
            LOG_DBG("%s",pParent.c_str());
        }

        LOG_DBG("Pseudochildrens:");

        for (auto pChild : i.pseudoChildrens)
        {
            LOG_DBG("%s",pChild.c_str());
        }

        LOG_DBG("");
        
    }
}

std::unordered_set<std::string> json_array_to_unordered_set(const Json::Value &values) {
    if (!values.isArray()) {
        return {};
    }

    std::unordered_set<std::string> result;
    for (const auto &value: values) {
        result.emplace(value.asString());
    }

    return result;
}

void Engine::apply_hard_constraints(const Json::Value& constraints) {
    std::string errors;
    Json::CharReader *reader = Json::CharReaderBuilder().newCharReader();

    for (auto constraint: constraints) {
        auto filter_key = dictionary[constraint["type"].asString()]["filter"].asString();
        auto filter_value = dictionary[constraint["type"].asString()]["value"];
        auto filter_value_as_set = json_array_to_unordered_set(filter_value);

        for (auto it = link_by_href.cbegin(); it != link_by_href.cend();) {
            request_t request;
            request.dst_host = it->second.host.c_str();
            request.dst_port = it->second.port.c_str();
            request.path = it->second.path.c_str();
            request.method = GET;
            request.query = std::string(filter_key + "=true").c_str();

            auto response = eddie_endpoint->get_client()->send_message_and_wait_response(request).data;
            Json::Value parsed_response;
            if (reader->parse(response.c_str(), response.c_str() + response.length(), &parsed_response, &errors)) {
                auto value_from_resource = parsed_response[filter_key].asString();

                if (filter_value_as_set.find(value_from_resource) == filter_value_as_set.end()) {
                    it = link_by_href.erase(it);
                }
            } else {
                it = link_by_href.erase(it);
            }

            it++;
        }
    }
}

std::multimap<int, Link> Engine::apply_objective_functions(const Json::Value& functions) {
    auto rankings = std::multimap<int, Link>();
    std::string errors;
    Json::CharReader *reader = Json::CharReaderBuilder().newCharReader();

    // prepare the query to obtain the values to rank the resources
    std::string o_f_query;
    for (auto function : functions) {
        auto feature_value = function["feature"].asString();
        auto feature_dict = dictionary[feature_value];
        o_f_query += feature_dict["query"].asString() + "&";
    }

    if (o_f_query.back() == '&') o_f_query.pop_back();

    /* there is no rank to apply:
     *   1. all resources go to the first rank
     *   2. return the ranking map
     */
    if (o_f_query.empty()) {
        for (const auto &resource: link_by_href) {
            rankings.emplace(0, resource.second);
        }

        return rankings;
    }

    for (auto resource = link_by_href.cbegin(); resource != link_by_href.cend();) {
        request_t request;
        request.dst_host = resource->second.host.c_str();
        request.dst_port = resource->second.port.c_str();
        request.path = resource->second.path.c_str();
        request.method = GET;
        request.query = o_f_query.c_str();

        auto response = eddie_endpoint->get_client()->send_message_and_wait_response(request).data;
        Json::Value parsed_response;

        if (reader->parse(response.c_str(), response.c_str() + response.length(), &parsed_response, &errors)) {
            int ranking = 0;
            for (int i = 0; i < functions.size(); i++) {
                auto feature = functions[i];
                auto feature_key = dictionary[feature["feature"].asString()]["key"].asString();
                auto feature_result = dictionary[feature["feature"].asString()]["result"].asString();

                if (parsed_response[feature_key] != feature_result) {
                    /*
                     * increase the ranking only if the feature requested is not provided by this resource
                     *
                     * since the ranking considered is only a categorical rank, only 2 possible outcomes are possible:
                     *    (the category is the same OR the category is NOT the same)
                     * thus the powers of 2 are used for ranking indexes
                     *
                     * since the first function has more ranking power, it has the highest exponent in the math formula
                     */
                    ranking += (int) pow(2, functions.size() - 1 - i);
                }

                rankings.emplace(ranking, resource->second);
            }
        } else {
            resource = link_by_href.erase(resource);
        }

        resource++;
    }

    return rankings;
}

std::string Engine::perform(const std::string &command) {
    Json::Value root;
    std::string errors;
    Json::CharReader *reader = Json::CharReaderBuilder().newCharReader();

    if (!reader->parse(command.c_str(), command.c_str() + command.length(), &root, &errors)) {
        return "ERROR WHILE PARSING COMMAND";
    }
    //LOG_DBG("Updating resource .....");
    //LOG_DBG("ip_Dir before update = %s", eddie_endpoint->get_resource_dir_ip().c_str());
    update_resources(); // -> resources in the net and jsons of every resource updated

    //LOG_DBG("updated resource");
    //LOG_DBG("ip_Dir after update = %s", eddie_endpoint->get_resource_dir_ip().c_str());

    std::string action = root["action"].asString();

    if (action == "find") 
    {
        LOG_DBG("Handling request of finding");
        std::string q;
        auto constraints = root["h_constraints"];

        for (const auto &constraint: constraints) {
            auto c_name = constraint.getMemberNames()[0].c_str();
            auto c_content = constraint[c_name].asString();

            q += std::string(c_name) + "=" + c_content + "&";
        }

        if (q.back() == '&') q.pop_back();

        request_t request;
        request.method = GET;
        request.path = "rd-lookup/res";
        request.query = q.c_str();
        //LOG_DBG("ip_Dir before assign = %s", eddie_endpoint->get_resource_dir_ip().c_str());
        std::string  ip = eddie_endpoint->get_resource_dir_ip();
        //LOG_DBG("ip_Dir after assign = %s", ip.c_str());
        request.dst_host = ip.c_str();
        request.dst_port = eddie_endpoint->get_resource_dir_port().c_str();

        //LOG_DBG("ip_Dir after assign in request = %s", eddie_endpoint->get_resource_dir_ip().c_str());
        //LOG_DBG("ip_Dir in request = %s", request.dst_host);
        LOG_DBG("Send Message to: %s:%s with query:%s and path:rd-lookup/res", request.dst_host, request.dst_port,q.c_str());

        return eddie_endpoint->get_client()->send_message_and_wait_response(request).data;
    }

    if (action == "selection")
    {
        LOG_DBG("Handling request of selection");

        auto variables = root["h_constraints"];
        auto constraints = root["o_function"];

        //LOG_DBG("sizes: %s", variables[0]["sizes"].asString().c_str());

        //Saves number of the variables to select
        int size = std::stoi(variables[0]["size"].asString());

        LOG_DBG("Saved size");

        std::unordered_map<std::string, std::vector<int>> m;
        std::unordered_map<std::string, std::vector<int>> constrsForNeighbor;

        for (int i = 1; i <= size; i++)
        {
            std::string ip = variables[i]["dst_ip"].asString();
            std::string port = variables[i]["dst_port"].asString();

            if (m.find(ip +"@"+ port) == m.end())
            {
                std::vector<int> val;
                val.push_back(i);
                std::string key = ip +"@"+ port;
                m.insert(std::make_pair(key, val));

                std::vector<int> indexsConstr;
                //Partitioning index of constraints and agents
                int j = 0;
                for (const auto &constraint:constraints)//("Con_name=exp").....(max/min=type)
                {
                    auto c_name = constraint.getMemberNames()[0].c_str();
                    auto c_content = constraint[c_name].asString();

                    if (std::string(c_name) != "max/min")
                    {
                        if (c_content.find("x" + std::to_string(i-1)) !=std::string::npos)
                            indexsConstr.push_back(j);
                        
                    }
                    
                    //q += std::string(c_name) + "=" + c_content + "&";
                    j++;
                }

                constrsForNeighbor.insert(std::make_pair(key,indexsConstr));
            }
            else
            {
                int j = 0;
                for (const auto &constraint:constraints)//("Con_name=exp").....(max/min=type)
                {
                    auto c_name = constraint.getMemberNames()[0].c_str();
                    auto c_content = constraint[c_name].asString();

                    if (std::string(c_name) != "max/min")
                    {
                        if (c_content.find("x" + std::to_string(i-1)) != std::string::npos && std::find(constrsForNeighbor.find(ip +"@"+ port)->second.begin(), constrsForNeighbor.find(ip +"@"+ port)->second.end(), j) == constrsForNeighbor.find(ip +"@"+ port)->second.end() )
                            constrsForNeighbor.find(ip +"@"+ port)->second.push_back(j);
                        
                    }
                    
                    //q += std::string(c_name) + "=" + c_content + "&";
                    j++;
                }

                (m.find(ip +"@"+ port))->second.push_back(i);
            }
            

        }

        //Creation of the constraints graph and DFS tree for DPOP
        //LOG_DBG("Creation ConstraintGraph");
        std::unordered_map<std::string, std::vector<std::string>> constrGraph = obtainConstraintGraph(constraints,constrsForNeighbor);
        LOG_DBG("ConstraintGraph created");
        printConstGraph(constrGraph);

        std::vector<NodeDFS> dfs = obtainDFS(constrGraph);

        LOG_DBG("DFS created \n");
        printDFS(dfs);

        //Send requests to configure each agents
        request_t request;
        std::string q = "";
        std::string dataConstr = "";
        std::string qGet = "";

        for (std::unordered_map<std::string, std::vector<int>>::iterator iter = m.begin(); iter != m.end(); iter++)
        {
            std::vector<int> indexes = iter->second;

            q = "";
            dataConstr = "";

            //Save id of the variables
            for (int v = 1; v <= size; v++)//("id=allarm")
            //for (std::vector<int>::iterator it = indexes.begin(); it != indexes.end(); it++)//("id=allarm")
            {
                q+= "id=" + variables[v]["id"].asString() + "&";
            }
            //Save the constraint and type of optimization
            for (const auto &constraintIndex:constrsForNeighbor.find(iter->first)->second)//("Con_name=exp").....(max/min=type)
            {
                auto c_name = constraints[constraintIndex].getMemberNames()[0].c_str();
                auto c_content = constraints[constraintIndex][c_name].asString();

                //LOG_DBG("name_const=%s, content=%s",c_name, c_content.c_str());
                dataConstr += std::string(c_name) + ":" + c_content + "&";
            }

            q+= "max/min=" + constraints[0]["max/min"].asString() + "&";

            //Save Ip and port of the agents for MGM
            for (std::unordered_map<std::string, std::vector<int>>::iterator neigh = m.begin(); neigh != m.end(); neigh++) //(neigh=ip:port)
            {
                std::string address = neigh->first;
                std::replace( address.begin(), address.end(), '%', '$');
                q+= "neigh=" + address + "&";
                //qGet += "neigh=" + neigh->first + "&";
            }

            /*Save Ip and port of the parent, pseudo-children, children and pseudo-parent of the agent iter
            for DPOP
            */

           /*

           */

            NodeDFS* node = fromNameToNodeDFS(iter->first,&dfs);

            std::string address = node->parent;
            //Find and save the index of the variables of the father
            std::vector<int> indexOfVariablesHandledByParentsAndPseudoParents; 
            
            if(address != "")
            {
                indexOfVariablesHandledByParentsAndPseudoParents = m.find(address)->second;
                std::replace( address.begin(), address.end(), '%', '$');
                q+= "parent=" + address + "&";
            }

            
            //q+= "childrens=[";
            for (auto i : node->childrens)
            {
                address = i;
                std::replace( address.begin(), address.end(), '%', '$');
                q+= "children=" + address + "&";
            }

            //if(q.back() == ',') q.pop_back();
            //q+= "]&";
            
            //q+= "pseudoChildrens=[";
            for (auto i : node->pseudoChildrens)
            {
                address = i;
                std::replace( address.begin(), address.end(), '%', '$');
                q+= "pseudoChildren=" + address + "&";
            }

            //if(q.back() == ',') q.pop_back();
            //q+= "]&";

            //q+= "pseudoParents=[";
            for (auto i : node->pseudoParents)
            {
                address = i;

                //Find and save the index of the variables of the pseudofather
                std::vector<int> indexes = m.find(address)->second;
                indexOfVariablesHandledByParentsAndPseudoParents.insert(indexOfVariablesHandledByParentsAndPseudoParents.end(),indexes.begin(),indexes.end());

                std::replace( address.begin(), address.end(), '%', '$');
                q+= "pseudoParent=" + address + "&";
            }

            //if(q.back() == ',') q.pop_back();
            //q+= "]&";     

            for (auto i: indexOfVariablesHandledByParentsAndPseudoParents)
            {
                q+= "indexP=" + std::to_string(i-1) + "&";
            }
                   

            if(q.back() == '&') q.pop_back();
            if(dataConstr.back() == '&') dataConstr.pop_back();

            auto ipAndPort = split(iter->first,'@');
            request.method = PUT;
            request.path = "MGM";
            request.query = q.c_str();
            request.dst_host = ipAndPort[0].c_str();
            request.dst_port = ipAndPort[1].c_str();
            request.data = reinterpret_cast<const uint8_t *>(dataConstr.c_str());
            request.data_length = dataConstr.length();
            
            LOG_DBG("Send Message to: %s@%s with query: %s and constrs: %s", ipAndPort[0].c_str(), ipAndPort[1].c_str(), q.c_str(),dataConstr.c_str());
            
            message_t response = eddie_endpoint->get_client()->send_message_and_wait_response(request);

            if (response.status_code == COAP_RESPONSE_CODE_BAD_REQUEST)
            {
                return "Error in the query: check if there are the resource and constraint in h_constraints and o_function";
            }
            
        }

        LOG_DBG("Setup completed:");

        //return "Breakpoint";

        q = "opt=START";

        //Case DPOP
        for (std::unordered_map<std::string, std::vector<int>>::iterator iter = m.begin(); iter != m.end(); iter++)
        {
            NodeDFS* node = fromNameToNodeDFS(iter->first,&dfs);
            //Only the leaf must start the algo with the Phase util
            if(node->childrens.size() == 0)
            {
                auto ipAndPort = split(iter->first,'@');
                request.method = POST;
                request.path = "MGM";
                request.query = q.c_str();
                request.dst_host = ipAndPort[0].c_str();
                request.dst_port = ipAndPort[1].c_str();

                LOG_DBG("Send Message to: %s@%s with query: %s", ipAndPort[0].c_str(), ipAndPort[1].c_str(), q.c_str());
                
                message_t response = eddie_endpoint->get_client()->send_message_and_wait_response(request);

                if (response.status_code == COAP_RESPONSE_CODE_BAD_REQUEST)
                {
                    return "Error in the query: check if there are the resource and constraint in h_constraints and o_function";
                }                
            }
        }

        //Case MGM
        /*for (std::unordered_map<std::string, std::vector<int>>::iterator iter = m.begin(); iter != m.end(); iter++)
        {
            auto ipAndPort = split(iter->first,'@');
            request.method = POST;
            request.path = "MGM";
            request.query = q.c_str();
            request.dst_host = ipAndPort[0].c_str();
            request.dst_port = ipAndPort[1].c_str();

            LOG_DBG("Send Message to: %s@%s with query: %s", ipAndPort[0].c_str(), ipAndPort[1].c_str(), q.c_str());
            
            message_t response = eddie_endpoint->get_client()->send_message_and_wait_response(request);

            if (response.status_code == COAP_RESPONSE_CODE_BAD_REQUEST)
            {
                return "Error in the query: check if there are the resource and constraint in h_constraints and o_function";
            }
        }*/

        LOG_DBG("Algo Started:");

        for (std::unordered_map<std::string, std::vector<int>>::iterator neigh = m.begin(); neigh != m.end(); neigh++)
        {
            NodeDFS* node = fromNameToNodeDFS(neigh->first,&dfs);

            if(node->childrens.size() == 0)//Case DPOP (only leaf)
                qGet += "neigh=" + neigh->first + "&";
        }
        qGet+= "time=" + std::to_string(SECONDS_TIMEOUT + 1);

        return qGet;

        /*LOG_DBG("Wait ....");
        sleep(SECONDS_TIMEOUT+1);
        LOG_DBG("Retriving values of the selection ...");

        q = "";
        std::vector<int> values;
        values.reserve(size);
        request_t request1;

        for (std::unordered_map<std::string, std::vector<int>>::iterator iter = m.begin(); iter != m.end(); iter++)
        {
            auto ipAndPort = split(iter->first,'@');
            request1.method = GET;
            request1.path = "MGM";
            //request1.query = q.c_str();
            request1.dst_host = ipAndPort[0].c_str();
            request1.dst_port = ipAndPort[1].c_str();

            LOG_DBG("Send Message to: %s@%s with query: %s", ipAndPort[0].c_str(), ipAndPort[1].c_str(), q.c_str());
            
            message_t response; 
            
            do
            {   
                response = eddie_endpoint->get_client()->send_message_and_wait_response(request1);
                //sleep(10);
            }while(response.status_code == COAP_RESPONSE_CODE_CONFLICT);

            std::string local = response.data;

            auto tokens = split(local,'&');

            for (auto token : tokens)
            {
                auto indexAndValue = split(token,'=');
                values[stoi(indexAndValue[0])] = stoi(indexAndValue[1]);
            }
            

            if (response.status_code == COAP_RESPONSE_CODE_BAD_REQUEST)
            {
                return "Error in the query: check if there are the resource and constraint in h_constraints and o_function";
            }
        }

        std::string strRes = "[";
        for(auto value : values)
            strRes+= std::to_string(value) + ',';

        strRes.pop_back();
        strRes.push_back(']');

        return strRes;*/
    }

    if (action =="GETselection")
    {
        LOG_DBG("Retriving values of the selection ...");
        auto param = root["h_constraints"];

        //LOG_DBG("sizes: %s", variables[0]["sizes"].asString().c_str());

        //Saves number of the variables to select
        int size = std::stoi(param[0]["size"].asString());

        std::vector<int> values;
        values.reserve(size);
        request_t request1;

        LOG_DBG("param.size() = %d",param.size());

        for (int i = 1; i <= param.size() -1; i++)
        {
            request1.method = GET;
            request1.path = "MGM";
            //request1.query = q.c_str();
            std::string ip = param[i]["dst_ip"].asString();
            std::string port = param[i]["dst_port"].asString();
            request1.dst_host = ip.c_str();
            request1.dst_port = port.c_str();

            LOG_DBG("Send Message to: %s@%s for Get selection", ip.c_str(), port.c_str());
            
            message_t response; 
            

            response = eddie_endpoint->get_client()->send_message_and_wait_response(request1);

            if(response.status_code == COAP_RESPONSE_CODE_CONFLICT)
                return "COAP_RESPONSE_CODE_CONFLICT";

            if (response.status_code == COAP_RESPONSE_CODE_BAD_REQUEST)
                return "[]"; 

            std::string local = response.data;

            //LOG_DBG("data=%s",local.c_str());

            auto tokens = split(local,'&');

            for (auto token : tokens)
            {
                auto indexAndValue = split(token,'=');
                values[stoi(indexAndValue[0])] = stoi(indexAndValue[1]);
                //LOG_DBG("%d=%d",stoi(indexAndValue[0]),values[stoi(indexAndValue[0])]);

            }
            

        }

        std::string strRes = "[";
        
        //LOG_DBG("values.size()=%ld",values.size());

        for(int k = 0; k < size;k++)
            strRes+= std::to_string(values[k]) + ",";

        //LOG_DBG("strRes=%s",strRes.c_str());
        strRes.pop_back();
        //LOG_DBG("strRes=%s",strRes.c_str());
        strRes+= ']';
        //LOG_DBG("strRes=%s",strRes.c_str());

        return strRes;
    }
    
    

    if (action == "get") {
        std::string q;
        auto req_resources = root["h_constraints"][3]["res"];
        for (const auto &req_resource: req_resources) {
            q += req_resource.asString() + "=true&";
        }
        if (q.back() == '&') q.pop_back();

        request_t request;
        request.method = GET;
        request.path = root["h_constraints"][2]["dst_res"].asCString();
        request.query = q.c_str();
        request.dst_host = root["h_constraints"][0]["dst_ip"].asCString();
        request.dst_port = root["h_constraints"][1]["dst_port"].asCString();

        return eddie_endpoint->get_client()->send_message_and_wait_response(request).data;
    }

    if (action == "actuate") {
        auto action_description = root["action_description"].asString();
        Json::Value answer;
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";

        // apply hard constraints;
        auto h_constraints = root["h_constraints"];
        apply_hard_constraints(h_constraints);

        auto o_functions = root["o_function"];
        auto rankings = apply_objective_functions(o_functions);

        // rankings contains the resources filtered by h_constraints and ordered by o_functions
        // complete the action
        request_t actuate_request;
        actuate_request.method = method_from_string(dictionary[action_description]["method"].asString());
        actuate_request.query = dictionary[action_description]["query"].asCString();

        // this is the reset request, which needs to run on all resources
        int cardinality = std::stoi(dictionary[action_description]["cardinality"].asString());
        if (cardinality < 0) {
            for (const auto &resource: rankings) {
                actuate_request.dst_host = resource.second.host.c_str();
                actuate_request.dst_port = resource.second.port.c_str();
                actuate_request.path = resource.second.path.c_str();

                eddie_endpoint->get_client()->send_message_and_wait_response(actuate_request);
            }

            answer["result"] = root["action_description"].asString() + " completed";
            return Json::writeString(builder, answer);
        }

        int count = 0;

        while (count < cardinality) {
            for (const auto &resource: rankings) {
                actuate_request.dst_host = resource.second.host.c_str();
                actuate_request.dst_port = resource.second.port.c_str();
                actuate_request.path = resource.second.path.c_str();

                auto response = eddie_endpoint->get_client()->send_message_and_wait_response(actuate_request);

                if (response.status_code <= CONTINUE) {
                    count++;
                }

                if (count == cardinality) {
                    answer["result"] = root["action_description"].asString() + " completed";
                    return Json::writeString(builder, answer);
                }
            }
        }

        answer["result"] = root["action_description"].asString() + " cannot be completed";
        return Json::writeString(builder, answer);
    }

    if (action == "actuateS") 
    {
        auto action_description = root["action_description"].asString();
        std::string q = dictionary[action_description]["query"].asCString();

        /*auto req_resources = root["h_constraints"][3]["query"];
        for (const auto &req_resource: req_resources) 
        {
            auto c_name = constraint.getMemberNames()[0].c_str();
            auto c_content = constraint[c_name].asString();

            q += std::string(c_name) + "=" + c_content + "&";
        }
        if (q.back() == '&') q.pop_back();*/

        request_t request;
        request.method = method_from_string(dictionary[action_description]["method"].asString());
        request.path = root["h_constraints"][2]["dst_res"].asCString();
        request.query = q.c_str();
        request.dst_host = root["h_constraints"][0]["dst_ip"].asCString();
        request.dst_port = root["h_constraints"][1]["dst_port"].asCString();
        LOG_DBG("Send Message to: %s@%s with query: %s",request.dst_host , request.dst_port, q.c_str());
        return eddie_endpoint->get_client()->send_message_and_wait_response(request).data;        
    }

    return "{}";
}

void Engine::update_resources() {

    //LOG_DBG("Start retriving resources from rd...");
    auto discover_results = eddie_endpoint->get_resources_from_rd();
    //LOG_DBG("Resources obtained");

    json_link_by_rt.clear();
    link_by_href.clear();

    for (const auto &result: discover_results) {
        request_t request;
        request.method = GET;
        request.dst_host = result.host.c_str();
        request.dst_port = result.port.c_str();
        request.path = result.path.c_str();
        request.query = "json=true";

        //LOG_DBG("Send Message to: %s:%s with query:json=true and path:%s", result.host.c_str(), result.port.c_str(),result.path.c_str());

        message_t response = eddie_endpoint->get_client()->send_message_and_wait_response(request);

        Json::Value root;
        std::string errors;
        Json::CharReader *reader = Json::CharReaderBuilder().newCharReader();

        bool success = reader->parse(response.data.c_str(),
                                     response.data.c_str() + response.data.length(),
                                     &root, &errors);

        if (success) {
            Json::CharReaderBuilder readerBuilder;
            readerBuilder["indentation"] = "";
            Json::Value tmp;

            reader->parse(root["json"].asCString(),
                          root["json"].asCString() + root["json"].asString().length(),
                          &tmp, &errors);

            auto rt = tmp["properties"]["rt"]["value"].asString();

            json_link_by_rt.emplace(rt, std::make_pair(tmp, result));
            link_by_href.emplace(result.host + ":" + result.port + "/" + result.path, result);
        } else {
            LOG_DBG("%s", errors.c_str());
        }
    }
}
