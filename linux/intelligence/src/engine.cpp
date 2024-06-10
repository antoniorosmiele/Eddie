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
    LOG_DBG("Updating resource .....");

    update_resources(); // -> resources in the net and jsons of every resource updated

    //LOG_DBG("updated resource");

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
        request.dst_host = eddie_endpoint->get_resource_dir_ip().c_str();
        request.dst_port = eddie_endpoint->get_resource_dir_port().c_str();

        LOG_DBG("Send Message to: %s:%s with query:%s and path:rd-lookup/res", request.dst_host, request.dst_port,q.c_str());

        return eddie_endpoint->get_client()->send_message_and_wait_response(request).data;
    }

    if (action == "selection")
    {
        LOG_DBG("Handling request of selection");

        auto variables = root["h_constraints"];
        auto constraints = root["o_function"];

        //LOG_DBG("sizes: %s", variables[0]["sizes"].asString().c_str());

        int size = std::stoi(variables[0]["size"].asString());

        LOG_DBG("Saved size");

        std::unordered_map<std::string, std::vector<int>> m;

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
            }
            else
            {
                (m.find(ip +"@"+ port))->second.push_back(i);
            }
            

        }

        request_t request;
        std::string q = "";

        for (std::unordered_map<std::string, std::vector<int>>::iterator iter = m.begin(); iter != m.end(); iter++)
        {
            std::vector<int> indexes = iter->second;

            //Save id of the variables
            for (std::vector<int>::iterator it = indexes.begin(); it != indexes.end(); it++)//("id=allarm")
            {
                q+= "id=" + variables[*it]["id"].asString() + "&";
            }
            //Save the constraint and type of optimization
            for (const auto &constraint:constraints)//("Con_name=exp").....(max/min=type)
            {
                auto c_name = constraint.getMemberNames()[0].c_str();
                auto c_content = constraint[c_name].asString();

                q += std::string(c_name) + "=" + c_content + "&";
            }
            //Save Ip and port of the agents
            for (std::unordered_map<std::string, std::vector<int>>::iterator neigh = m.begin(); neigh != m.end(); neigh++) //(neigh=ip:port)
            {
                q+= "neigh=" + neigh->first + "&";
            }
            
            

            if(q.back() == '&') q.pop_back();

            auto ipAndPort = split(iter->first,'@');
            request.method = PUT;
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

        LOG_DBG("Setup completed:");

        q = "opt=START";

        for (std::unordered_map<std::string, std::vector<int>>::iterator iter = m.begin(); iter != m.end(); iter++)
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
        
        
        return "Breakpoint";


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

        LOG_DBG("Send Message to: %s:%s with query:json=true and path:%s", result.host.c_str(), result.port.c_str(),result.path.c_str());

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
