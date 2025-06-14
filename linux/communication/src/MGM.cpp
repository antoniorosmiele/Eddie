#include "MGM.h"
#include "common.h"
#include <math.h>  
#include <coap3/coap.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <arpa/inet.h>
#include <map>
#include <coap3/uri.h>

inline const std::string  BoolToString(bool b)
{
  return b ? "true" : "false";
}

inline const bool  StringToBool(std::string s)
{
  return (s == "true") ? true : false;
}


void MGM::MGM_put(coap_resource_t *resource, coap_session_t *session, const coap_pdu_t *request, const coap_string_t *query, coap_pdu_t *response)
{
    LOG_DBG("Handling message PUT for MGM");
    //Check if the query is empty
    if (!query) 
    {
        LOG_ERR("Error: empty query");
        coap_pdu_set_code(response, COAP_RESPONSE_CODE_BAD_REQUEST);
        return;
    }

    //Count the number of segment in the query
    size_t buf_len = query->length + 128;
    unsigned char buf[buf_len];
    int n_opts = coap_split_query((const uint8_t *) query->s, query->length, buf, &buf_len);

    LOG_DBG("query=%s", std::string(reinterpret_cast<const char *>(query->s), query->length).c_str());
    //If there are no segments return an error message
    if (n_opts < 0) {
        LOG_ERR("Problem parsing query");
        coap_pdu_set_code(response, COAP_RESPONSE_CODE_BAD_REQUEST);
        return;
    }

    //Obtain the subquery and save them 
    std::vector<std::string> sub_queries;
    unsigned char *p_buf = buf;
    while (n_opts--) {
        std::string option = std::string(reinterpret_cast<const char *>(coap_opt_value(p_buf)), coap_opt_length(p_buf));
        sub_queries.emplace_back(option);
        p_buf += coap_opt_size(p_buf);
    }

    std::unordered_map<std::string, std::string> query_map_constraint;
    std::vector<std::string> ids;
    std::vector<std::string> neighbors;
    bool maxOrMin;
    //datas for the DPOP algo
    std::string parent = "";
    std::vector<std::string> pseudoParents;
    std::vector<std::string> pseudoChildrens;
    std::vector<std::string> childrens;
    std::vector<int> indexOfVariablesHandledByParentsAndPseudoParents;

    unsigned char tmp_buf[64];
    char str_buff[20];

    //Obtain local address
    const coap_address_t* address = coap_session_get_addr_local(session);
    size_t length = coap_print_addr(address, tmp_buf, 64);
    std::string base_address = std::string(reinterpret_cast<const char *>(tmp_buf), length);
    std::string onlyAddress = split(base_address,']')[0].substr(1);
    LOG_DBG("Obtained address local: %s",base_address.c_str());
    LOG_DBG("Obtained address: %s",onlyAddress.c_str());
            
    //obtain port
    uint16_t port_int = coap_address_get_port(address);
    int len = sprintf(str_buff, "%d", port_int);

    std::string base_port = std::string(reinterpret_cast<const char *>(str_buff), len);
    LOG_DBG("Obtained port: %s", base_port.c_str());

    // get data with constraints and save them
    size_t data_len, offset, total;
    const uint8_t *data;
    coap_get_data_large(request, &data_len, &data, &offset, &total);       
    //LOG_DBG("data=%s", std::string(reinterpret_cast<const char *>(data), data_len).c_str()); 

    std::vector<std::string> sub_constrs = split(std::string(reinterpret_cast<const char *>(data), data_len),'&');
    
    for (const auto &token: sub_constrs)
    {
        std::vector<std::string> sub_const = split(token, ':');
        LOG_DBG("Saving constraint %s=%s",sub_const[0].c_str(), sub_const[1].c_str());
        query_map_constraint.insert(std::make_pair(sub_const[0], sub_const[1]));  // For the constraint ("Con_name=exp")
    } 

    for ( auto &token: sub_queries) 
    {
        std::vector<std::string> sub_query = split(token, '=');
    
        if(sub_query[0].compare("id") == 0)
            ids.emplace_back(sub_query[1]); //For the id of resource ("id=allarm")
        else if (sub_query[0].compare("neigh") == 0) // For the ip and port of the agents (neigh=ip:port)
        {
            std::replace( sub_query[1].begin(), sub_query[1].end(), '$', '%');
            std::vector<std::string> ipAndPort = split(sub_query[1], '@');

            //LOG_DBG("Comparing neighbor: %s@%s",split(ipAndPort[0],'%')[0].c_str(),ipAndPort[1].c_str());

            //if(split(ipAndPort[0],'%')[0] != onlyAddress && ipAndPort[1] != base_port)

            std::string neigh = split(ipAndPort[0],'%')[0] + "@" + ipAndPort[1];
            std::string ipCurr = onlyAddress + "@" +base_port;

            LOG_DBG("Comparing neighbor: %s with %s compare = %d",neigh.c_str(),ipCurr.c_str(),neigh.compare(ipCurr));

            if(!(neigh.compare(ipCurr) == 0))
            {
                neighbors.emplace_back(ipAndPort[0] + "@" + ipAndPort[1]);
                LOG_DBG("Saving neighbor: %s@%s",ipAndPort[0].c_str(),ipAndPort[1].c_str());
            }
            
        }
        else if (sub_query[0].compare("max/min") == 0) //For the type of optimisation (max/min=type)
        {
            if (sub_query[1].compare(MGM_MAX) == 0)
                maxOrMin =true;
            else
                maxOrMin = false;
        }
        else if (sub_query[0].compare("parent") == 0)
        {
            std::replace( sub_query[1].begin(), sub_query[1].end(), '$', '%');
            parent = sub_query[1];
        }
        else if (sub_query[0].compare("children") == 0)
        {
            std::replace( sub_query[1].begin(), sub_query[1].end(), '$', '%');
            childrens.emplace_back(sub_query[1]);
        }
        else if (sub_query[0].compare("pseudoChildren") == 0)
        {
            std::replace( sub_query[1].begin(), sub_query[1].end(), '$', '%');
            pseudoChildrens.emplace_back(sub_query[1]);
        }
        else if (sub_query[0].compare("pseudoParent") == 0)
        {
            std::replace( sub_query[1].begin(), sub_query[1].end(), '$', '%');
            pseudoParents.emplace_back(sub_query[1]);            
        }
        else if (sub_query[0].compare("indexP") == 0)
        {
            indexOfVariablesHandledByParentsAndPseudoParents.emplace_back(stoi(sub_query[1]));
        }
        
        
        
        
           
    }

    //If there aren't no ids of the variables then send an error message
    if(ids.size() == 0)
    {
        LOG_ERR("Error: there aren't no ids of the variables");
        coap_pdu_set_code(response, COAP_RESPONSE_CODE_BAD_REQUEST);
        return;        
    }

    //Obtain pointer of the object MGM saved in the coap resource MGM
    MGM * mgm = (MGM * ) coap_resource_get_userdata(resource);

    //Obtain local resource in link format
    std::string local_resource = mgm->commAndRes->get_resources_in_linkformat();
    std::vector<Link> links = parse_link_format(local_resource);
    
    //Save in an other Array inside MGM the variables handled by this agent
    for (const auto &link: links) 
    {   
        //LOG_DBG("1");
        LOG_DBG("Finding id local: %s of rt = %s",link.attrs.find("id")->second.c_str(),link.attrs.find("rt")->second.c_str());
        std::string id_local = link.attrs.find("id")->second;
        std::vector<std::string>::iterator elem = std::find(ids.begin(), ids.end(), id_local);

        if ( elem != ids.end() )
        {   
            int index = elem - ids.begin();
            //LOG_DBG("Save variables local with id:%s and index:%d",(*elem).c_str(),index);
            mgm->indexOfVariablesHandled.emplace_back(index);
            //LOG_DBG("2");
            mgm->variablesHandled.emplace_back(link);
            //LOG_DBG("3");
        }    

    }

    //Save in an Array inside MGM the ids of the variables, initialise the value to false and save the ips and ports of the Agents

    for (const auto &id: ids)
    {
        LOG_DBG("Save variables with id:%s", id.c_str());
        mgm->allVariables.emplace_back(id);
        mgm->valuesVariables.emplace_back(false);
    }

    LOG_DBG("Saving ip and port of neighbors....");
    copy(neighbors.begin(), neighbors.end(), back_inserter(mgm->allNeighbors));

    //Save local ip and port
    mgm->base_address = onlyAddress;
    mgm->base_port = base_port;

    //Save the constraints ...
    mgm->isMax = maxOrMin;
    LOG_DBG("Save type of optimisation: %s", maxOrMin ? "Max" : "Min");

    mgm->query_map_constraint = query_map_constraint;

    for (auto &i : mgm->query_map_constraint)
    {
        LOG_DBG("Saved constraint: %s = %s", i.first.c_str(), i.second.c_str());
    }
    
    //Save the parameters for DPOP algo in mgm
    mgm->parent = parent;
    mgm->pseudoParents = pseudoParents;
    mgm->pseudoChildrens = pseudoChildrens;
    mgm->childrens = childrens;
    mgm->counterMsgValue = 0;
    mgm->indexOfVariablesHandledByParentsAndPseudoParents = indexOfVariablesHandledByParentsAndPseudoParents;

    //........................

    //Modify response message...

    coap_pdu_set_code(response, COAP_RESPONSE_CODE_CREATED);
}

void MGM::MGM_post(coap_resource_t *resource, coap_session_t *session, const coap_pdu_t *request, const coap_string_t *query, coap_pdu_t *response)
{
    LOG_DBG("Handling message Post for MGM");

    //Check if the query is empty
    if (!query) 
    {
        LOG_ERR("Error: empty query");
        coap_pdu_set_code(response, COAP_RESPONSE_CODE_BAD_REQUEST);
        return;
    }

    //Count the number of segment in the query
    size_t buf_len = query->length + 128;
    unsigned char buf[buf_len];
    int n_opts = coap_split_query((const uint8_t *) query->s, query->length, buf, &buf_len);

    //possible query:
    // "opt=START"
    // "opt=UPDATE&3=true&7=false&11=true"
    // "opt=UPDATEG&gain=60"

    //If there are no segments return an error message
    if (n_opts < 0) {
        LOG_ERR("Problem parsing query");
        coap_pdu_set_code(response, COAP_RESPONSE_CODE_BAD_REQUEST);
        return;
    }

    //Obtain the subquery and save them 
    std::vector<std::string> sub_queries;
    unsigned char *p_buf = buf;
    while (n_opts--) {
        std::string option = std::string(reinterpret_cast<const char *>(coap_opt_value(p_buf)), coap_opt_length(p_buf));
        sub_queries.emplace_back(option);
        p_buf += coap_opt_size(p_buf);
    }

    std::unordered_map<std::string, std::string> query_map;
    for (const auto &token: sub_queries) {
        std::vector<std::string> sub_query = split(token, '=');
        query_map.insert(std::make_pair(sub_query[0], sub_query[1]));
    }

    //Obtain pointer of the object MGM saved in the coap resource MGM
    MGM * mgm = (MGM * ) coap_resource_get_userdata(resource);

    //Obtain attribute opt from the query
    auto opt = query_map.find("opt");

    if (opt == query_map.end()) {
        coap_pdu_set_code(response, COAP_RESPONSE_CODE_BAD_REQUEST);
        return;
    }    

    //If opt is the command START then starts the execution of the MGM algorithm in this Agent
    if (opt->second == "START")
    {
        //LOG_DBG("Starting MGM...");
        //mgm->executeAlgo();

        LOG_DBG("Starting Util phase of DPOP...");
        auto mgm_task = [mgm]() 
        {
            mgm->dpopUtilLeaf(std::vector<std::string>());

            /*
                If the node is a root and there aren't children 
                then execute Value Phase        
            */
            if(mgm->parent == "" && mgm->childrens.size() == 0)
                mgm->dpopValue();             
        };

        mgm->done = false; //Says if the algo is finished and the results are ready 
        mgm->mgm_thread = std::thread(mgm_task);
               

        coap_pdu_set_code(response, COAP_RESPONSE_CODE_CREATED);
        return;        
    }
    else if (opt->second == "UPDATE")//If opt is the command UPDATE then save the new values of the variables in the class MGM
    {
        query_map.erase("opt");
        LOG_DBG("Saving new values of variables....");
        //Need a mutex
        {
            mgm->saveSomeValuesOfVariables(query_map);
        }
        coap_pdu_set_code(response, COAP_RESPONSE_CODE_CREATED);
        return;
    }
    else if (opt->second == "UPDATEG")
    {
        LOG_DBG("Saving new Gain:");
        auto gainStr = query_map.find("gain");
        
        if (gainStr == query_map.end()) 
        {
            coap_pdu_set_code(response, COAP_RESPONSE_CODE_BAD_REQUEST);
            return;
        }

        {
            LOG_DBG("%ld", stol(gainStr->second));
            mgm->saveGainFromNeighbor(stol(gainStr->second));
        }

        coap_pdu_set_code(response, COAP_RESPONSE_CODE_CREATED);
        return;

    }
    else if (opt->second == "UTIL")
    {
        query_map.erase("opt");
        
        auto dpop_task = [mgm,request,query_map]() 
        {
            // get data with all the rows of the util table and save them
            size_t data_len, offset, total;
            const uint8_t *data;
            coap_get_data_large(request, &data_len, &data, &offset, &total);       
            LOG_DBG("dataUtilReceive=%s", std::string(reinterpret_cast<const char *>(data), data_len).c_str()); 

            std::vector<std::string> allRows = split(std::string(reinterpret_cast<const char *>(data), data_len),'$');

            std::unordered_map<std::string, double> tableUtil;

            for (auto row : allRows)
            {
                std::vector<std::string> variablesAndUtil = split(row,':');

                tableUtil.insert({variablesAndUtil[0],std::stod(variablesAndUtil[1])});
            }

            std::vector<std::string> constraints_shared_in;

            for (auto constr_name:query_map)
                constraints_shared_in.push_back(constr_name.first);
            
            //Save util Talbe
            mgm->allUtilMsgFromChild.push_back(tableUtil);    

            //From this point must be executed in a thread!

            // Execute Util phaase and check if this node must send util messages to parent
            mgm->dpopUtilLeaf(constraints_shared_in); 

            /*
                If the node is a root and Util msg from children has been arrived 
                then execute Value Phase        
            */
            if(mgm->parent == "" && mgm->childrens.size() == mgm->allUtilMsgFromChild.size())
            {                
                //Nel caso della root la tableUtil diventa subito la tableValue
                for (auto table:mgm->allUtilMsgFromChild)
                {
                    for (auto row = table.begin(); row != table.end(); row++)
                    {
                        mgm->tableValue.insert({row->first,row->second}); 
                    }
                }
                
                mgm->dpopValue();
            }    
        };
        
        mgm->mgm_thread = std::thread(dpop_task);
        coap_pdu_set_code(response, COAP_RESPONSE_CODE_CREATED);
        return;          
    }
    else if (opt->second == "VALUE")
    {
        size_t data_len, offset, total;
        const uint8_t *data;
        coap_get_data_large(request, &data_len, &data, &offset, &total);
        LOG_DBG("dataValue=%s", std::string(reinterpret_cast<const char *>(data), data_len).c_str());
        std::vector<std::string> indexesAndGain = split(std::string(reinterpret_cast<const char *>(data), data_len),':');
        
        auto dpop_task = [mgm,indexesAndGain]() 
        {    
            std::vector<std::string> indexesAndValues = split(indexesAndGain[0],',');
            
            for (auto var : indexesAndValues)
            {
                std::vector<std::string> indexAndValue = split(var,'=');
                std::string index = indexAndValue[0];
                bool value = StringToBool(indexAndValue[1]);

                if(mgm->allValuesMsg.find(index) == mgm->allValuesMsg.end())
                {    
                    mgm->allValuesMsg.insert({index,value});
                }
            }

            mgm->counterMsgValue++;

            if(mgm->counterMsgValue == mgm->pseudoParents.size() + 1)
                mgm->dpopValue();
        };    

        if (mgm->mgm_thread1.joinable())
            mgm->mgm_thread1.join();
        
        mgm->mgm_thread1 = std::thread(dpop_task);
        coap_pdu_set_code(response, COAP_RESPONSE_CODE_CREATED);
        return;         
    }
    else
    {
        LOG_ERR("Error: opt not present or not correct");
        coap_pdu_set_code(response, COAP_RESPONSE_CODE_BAD_REQUEST);
        return;
    }
    
    
    
    

}

void MGM::MGM_get(coap_resource_t *resource, coap_session_t *session, const coap_pdu_t *request, const coap_string_t *query, coap_pdu_t *response)
{
    LOG_DBG("Handling message GET for MGM");

    //Obtain pointer of the object MGM saved in the coap resource MGM
    MGM * mgm = (MGM * ) coap_resource_get_userdata(resource);

    if (!mgm->done)
    {
        LOG_ERR("Error: algo MGM still running");
        coap_pdu_set_code(response, COAP_RESPONSE_CODE_CONFLICT);
        return;
    }
    

    std::vector<int> indexes = mgm->indexOfVariablesHandled;
    std::vector<bool> values = mgm->valuesVariables;

    std::string responseStr = mgm->bestIndexAndValues;

    std::string trueS = "true";
    std::string one = "1";
    std::string falseS = "false";
    std::string zero = "0";

    size_t pos = 0;
    while ((pos = responseStr.find("true", pos)) != std::string::npos) 
    {
        responseStr.replace(pos, trueS.length(), one);
        pos += one.length();
    }

    pos = 0;
    while ((pos = responseStr.find("false", pos)) != std::string::npos) 
    {
        responseStr.replace(pos, falseS.length(), zero);
        pos += zero.length();
    }

    /*for (auto i : indexes) For MGM
    {
        std::string value;

        if (values[i])
            value = "1";
        else
            value = "0";    

        responseStr+= std::to_string(i) + "=" + value + "&";
    }

    responseStr.pop_back();*/

    coap_pdu_set_code(response, COAP_RESPONSE_CODE_CONTENT);

    coap_add_data_large_response(resource,session,request,response,query,COAP_MEDIATYPE_TEXT_PLAIN, 1, 0, responseStr.length(), coap_make_str_const(responseStr.c_str())->s,NULL, NULL);
    
}

void MGM::MGM_delete(coap_resource_t *resource, coap_session_t *session, const coap_pdu_t *request, const coap_string_t *query, coap_pdu_t *response)
{

}

MGM::MGM()
{
    
}

MGM::MGM(coap_context_t *server_context,CoapServer *server)
{
    //Save Coap Context and Coap Server
    this->context = server_context;
    this->commAndRes = server;

    //Initialize and register MGM as coap resource
    std::string s = "MGM";
    coap_str_const_t* uri_path = coap_make_str_const(s.c_str());
    coap_resource_t *new_coap_resource = coap_resource_init(uri_path, 0);
    

    coap_register_handler(new_coap_resource, COAP_REQUEST_GET, MGM_get);
    coap_register_handler(new_coap_resource, COAP_REQUEST_PUT, MGM_put);
    coap_register_handler(new_coap_resource, COAP_REQUEST_POST, MGM_post);
    coap_register_handler(new_coap_resource, COAP_REQUEST_DELETE, MGM_delete);    
    coap_add_attr(new_coap_resource, coap_make_str_const("rt"), coap_make_str_const("\"core.mgm\""), 0);
    coap_resource_set_userdata(new_coap_resource, this);
    coap_add_resource(this->context, new_coap_resource);

    //initialise other variables of the class
    this->allVariables = {};
    this->valuesVariables = {};
    this->variablesHandled = {};
    this->indexOfVariablesHandled = {};
    this->allNeighbors = {};
    this->indexAndValues_vector = {};
    this->gain = 0;
}

void MGM::executeAlgo()
{
    auto mgm_task = [this]() 
    {
        this->mgmAlgo();
    };
    
    this->mgm_thread = std::thread(mgm_task);
    this->done = false;    
}

void MGM::mgmAlgo()
{
    LOG_DBG("Initilise local variables:");

    for (size_t i = 0; i < this->variablesHandled.size(); i++)
    {
        int index = this->indexOfVariablesHandled[i];
        //this->valuesVariables[index] = rand()%2;
        this->valuesVariables[index] = false;
        LOG_DBG("Variables with id %s with index %d has value %s", this->allVariables[index].c_str() , index, this->valuesVariables[index] ? "true" : "false");
    }

    gettimeofday(&tv, &tz);
    LOG_DBG("Starting timer......");
    start_time = tv.tv_sec + 0.000001*tv.tv_usec;

    long double curr_time = start_time;
    long count_iteration = 0;

    while (count_iteration <= MAX_ITERATIONS)
    {
        LOG_DBG("New step:");
        //SendValueMessage(allNeighbors, currentValue)
        LOG_DBG("Sending values of the local variables to the neighbors");
        bool status = SendValueMessage(allNeighbors,valuesVariables,indexOfVariablesHandled);

        bool done = false;

        //currentContext = getValueMessages(allNeighbor)
        while (!done)
        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock,[&](){return indexAndValues_vector.size() == allNeighbors.size();});
            LOG_DBG("Getting values of the local variables of the neighbors");
            getValueMessages(&valuesVariables,indexAndValues_vector);
            done = true;
            cv.notify_all();
        }

        this->indexAndValues_vector.clear();
        
        //[gain,newValue] = BestUnilateralGain(currentContext)
        std::vector<bool> newValues = {};

        for (auto &index : indexOfVariablesHandled)
        {
            newValues.push_back(false);

        }

        LOG_DBG("Obtaining best unilateral Gain..");
        long gainDelta = BestUnilateralGain(valuesVariables,indexOfVariablesHandled, &newValues) - this->gain;

        LOG_DBG("gain=%ld",gainDelta);

        //SendGainMessage(allNeighbors, gain)
        LOG_DBG("Send best gain to neighbors...");
        status = SendGainMessage(allNeighbors,gainDelta);

        //neighborsGains = ReceiveGainMessages(allNeighbors)
        done = false;

        while (!done)
        {
            std::unique_lock<std::mutex> lock(mtx);
            LOG_DBG("Waiting new gains by neighbors");
            cv.wait(lock,[&](){return gains_vector.size() == allNeighbors.size();});
            
            done = true;
            cv.notify_all();
            LOG_DBG("new gains by neighbors received");
        }
        //Controllare i Gain per vedere se arrivano tutti
        if ((gains_vector.size() != 0 && gainDelta > *std::max_element(gains_vector.begin(), gains_vector.end())) || (gains_vector.size() == 0))
        {
            LOG_DBG("New local gain is the max gain");
            this->gain+=gainDelta;
            
            int i = 0;

            for (auto &index : this->indexOfVariablesHandled)
            {
                LOG_DBG("Variables with index %d , id=%s has values %s now",index,this->allVariables[index].c_str(),newValues[i] ? "true" : "false");
                this->valuesVariables[index] = newValues[i];
                i++;
            }
            

        } 
        gains_vector.clear();
        //  currentValue = newValue
        //end if
        gettimeofday(&tv, &tz);
        curr_time = tv.tv_sec + 0.000001*tv.tv_usec;
        count_iteration++;
        LOG_DBG("End step");
    }

    LOG_DBG("count_iteration: %ld",count_iteration);

    this->done = true;
        
}

void MGM::saveSomeValuesOfVariables(std::unordered_map<std::string, std::string> values_map)
{
    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock,[&](){return indexAndValues_vector.size()< allNeighbors.size();});
        this->indexAndValues_vector.push_back(values_map);
        cv.notify_all();
        LOG_DBG("Saved");
    }
}

void MGM::saveGainFromNeighbor(long gain)
{
    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock,[&](){return this->gains_vector.size()< allNeighbors.size();});
        this->gains_vector.push_back(gain);
        cv.notify_all();
    }
}

bool MGM::SendGainMessage(std::vector<std::string> allNeighbors, long gain)
{
    //Send the new values of some variables to all the agent in allNeighbors
    for (const auto &agent: allNeighbors) 
    {
        //Create the query
        request_t request;
        std::string q = "opt=UPDATEG&gain=" + std::to_string(gain);

        //Create the request and receive the response
        std::vector<std::string> ipAndPort = split(agent, '@');

        request.method = POST;
        request.path = "MGM";
        request.query = q.c_str();
        request.dst_host = ipAndPort[0].c_str();
        request.dst_port = ipAndPort[1].c_str();

        LOG_DBG("Send Message to: %s@%s with query: %s", ipAndPort[0].c_str(), ipAndPort[1].c_str(), q.c_str());
        message_t response = commAndRes->get_client()->send_message_and_wait_response(request);

        //Handle the response
        
        if (response.status_code == COAP_RESPONSE_CODE_BAD_REQUEST)
        {
            return false;
        }
    }

    return true;    
}

bool MGM::SendValueMessage(std::vector<std::string> allNeighbors, std::vector<bool> valuesVariables, std::vector<int> indexOfVariablesHandled)
{
    //Send the new values of some variables to all the agent in allNeighbors
    for (const auto &agent: allNeighbors) 
    {
        //Create the query
        request_t request;
        std::string q = "opt=UPDATE&";

        for (std::vector<int>::iterator iter = indexOfVariablesHandled.begin(); iter != indexOfVariablesHandled.end(); iter++)
        {
            q+= std::to_string(*iter) + "=" + BoolToString(valuesVariables[*iter]) + "&"; // "opt=UPDATE&3=true&7=false&11=true"
        }
        
        if(q.back() == '&')
               q.pop_back();

        //Create the request and receive the response
        std::vector<std::string> ipAndPort = split(agent, '@');

        request.method = POST;
        request.path = "MGM";
        request.query = q.c_str();
        request.dst_host = ipAndPort[0].c_str();
        request.dst_port = ipAndPort[1].c_str();

        LOG_DBG("Send Message to: %s@%s with query: %s", ipAndPort[0].c_str(), ipAndPort[1].c_str(), q.c_str());
        message_t response = commAndRes->get_client()->send_message_and_wait_response(request);

        //Handle the response
        
        if (response.status_code == COAP_RESPONSE_CODE_BAD_REQUEST)
        {
            return false;
        }    
    }

    return true;
}

bool MGM::getValueMessages(std::vector<bool> * valuesVariables, std::vector<std::unordered_map<std::string, std::string>> indexAndValues_vector)
{
    for (auto &map : indexAndValues_vector)
    {
        for(std::unordered_map<std::string, std::string>::iterator it = map.begin(); it != map.end(); ++it) 
        {
            LOG_DBG("Variables with index %s , id=%s has values %s now",it->first.c_str(),this->allVariables[stoi(it->first)].c_str(),it->second.c_str());
            (*valuesVariables)[stoi(it->first)] = StringToBool(it->second);
        }
    }

    return true;
    
}

bool isOperator(char c)
{
    // Returns true if the character is an operator
    return c == '+' || c == '-' || c == '*' || c == '/'
           || c == '^' || c == '=';
}

// Function to get the precedence of an operator
int precedence(char op)
{
    // Returns the precedence of the operator
    if (op == '+' || op == '-')
        return 1;
    if (op == '*' || op == '/')
        return 2;
    if (op == '^')
        return 3;
    return 0;
}

// Function to apply an operator to two operands
double MGM::applyOp(double a, double b, char op)
{
    // Applies the operator to the operands and returns the
    // result
    switch (op) {
    case '+':
        return a + b;
    case '-':
        return a - b;
    case '*':
        return a * b;
    case '/':
        return a / b;
    case '^':
        return pow(a, b);
    case '=':

        if (!(this->isMax))
        {
            if (a == b)
                return 0;
            else
                return 10000.0;
        }
        else
        {
            if (a == b)
                return 10000.0;
            else
                return 0;
        }
            
            
    default:
        return 0;
    }
}

// Function to parse and evaluate a mathematical expression
double MGM::evaluateExpression(const std::string& expression, std::vector<bool> valuesVariables)
{
    std::stack<char> operators; // Stack to hold operators
    std::stack<double> operands; // Stack to hold operands

    std::stringstream ss(expression); // String stream to parse
                                 // the expression

    std::string token;
    while (getline(
        ss, token,
        ' ')) { // Parse the expression token by token

        if (token.empty())
            continue; // Skip empty tokens
        if (isdigit(token[0])) { // If the token is a number
            double num;
            std::stringstream(token)
                >> num; // Convert the token to a number
            operands.push(num); // Push the number onto the
                                // operand stack
        }
        else if (token[0] == 'x' && isdigit(token[1])) // If the token is a variable
        {
            int num;
            
            std::stringstream(token.substr(1))
                >> num; // Convert the portion of the token with the index to a number

            bool val = valuesVariables[num];

            operands.push(val ? 1.0:0); // Push the boolean converted in double onto the
                                        // operand stack            

        }
        else if (token.find("strcmp(") != std::string::npos)
        {
            //strcmp(value1,value2)
            /* evaluate strcmp() */
            //obtain "value1,value2"
            token.pop_back();
            auto  values = split(token.substr(7), ',');

            if (values[0] == values[1])
                operands.push(1.0);
            else
                operands.push(0);
            
        }
        else if (isOperator(token[0])) { // If the token is
                                         // an operator
            char op = token[0];
            // While the operator stack is not empty and the
            // top operator has equal or higher precedence
            while (!operators.empty()
                   && precedence(operators.top())
                          >= precedence(op)) {
                // Pop two operands and one operator
                double b = operands.top();
                operands.pop();
                double a = operands.top();
                operands.pop();
                char op = operators.top();
                operators.pop();
                // Apply the operator to the operands and
                // push the result onto the operand stack
                operands.push(applyOp(a, b, op));
            }
            // Push the current operator onto the operator
            // stack
            operators.push(op);
        }
        else if (token[0] == '(') { // If the token is an
                                    // opening parenthesis
            // Push it onto the operator stack
            operators.push('(');
        }
        else if (token[0] == ')') { // If the token is a
                                    // closing parenthesis
            // While the operator stack is not empty and the
            // top operator is not an opening parenthesis
            while (!operators.empty()
                   && operators.top() != '(') {
                // Pop two operands and one operator
                double b = operands.top();
                operands.pop();
                double a = operands.top();
                operands.pop();
                char op = operators.top();
                operators.pop();
                // Apply the operator to the operands and
                // push the result onto the operand stack
                operands.push(applyOp(a, b, op));
            }
            // Pop the opening parenthesis
            operators.pop();
        }
    }

    // While the operator stack is not empty
    while (!operators.empty()) {
        // Pop two operands and one operator
        double b = operands.top();
        operands.pop();
        double a = operands.top();
        operands.pop();
        char op = operators.top();
        operators.pop();
        // Apply the operator to the operands and push the
        // result onto the operand stack
        operands.push(applyOp(a, b, op));
    }

    // The result is at the top of the operand stack
    return operands.top();
}


long MGM::applyConstraint(std::unordered_map<std::string, std::string> constraints,std::vector<bool> valuesVariables,bool isMax)
{
    /*if (valuesVariables[2])
    {
        LOG_DBG("Valid");
        return 1;
    }

    return -9999999;*/

    double cost = 0;
    
    for (auto &i : constraints)
    {
        cost+=this->evaluateExpression(i.second,valuesVariables);
    }

    return cost;    
}

long MGM::BestUnilateralGain(std::vector<bool> valuesVariables, std::vector<int> indexOfVariablesHandled, std::vector<bool>* newValues)
{
    std::vector<bool> currentValues;
    //currentValues.reserve(indexOfVariablesHandled.size());

    int i = 0;

    for (auto &index : indexOfVariablesHandled)
    {
        currentValues.push_back(false);
        i++;
    }

    long currentGain;
    bool first = true;
    //
    for (int j = 0; j < pow(2,currentValues.size()); j++)
    {
        for (int k = 0; k < currentValues.size(); k++)
        {
            currentValues[k] = j & (1<< k);
        }
        
        /*printf("[");
        for (auto var : currentValues)
            printf("%s,", var ? "true" : "false");    
        LOG_DBG("]");*/
        
        i=0;

        for (auto &index : indexOfVariablesHandled)
        {
            valuesVariables[index] = currentValues[i];
            i++;
        }

        long newGain;
        newGain = applyConstraint(this->query_map_constraint,valuesVariables, this->isMax);

        if (first)
        {
            currentGain = newGain;
            first = false;
            i = 0;
            for (auto &index : indexOfVariablesHandled)
            {
                newValues->operator[](i) = valuesVariables[index];
                i++;    
            }           
        }
        else if (this->isMax)
        {
            if (newGain > currentGain)
            {   
                LOG_DBG("New gain max: %ld",newGain);
                currentGain = newGain;
                i = 0;
                for (auto &index : indexOfVariablesHandled)
                {
                    newValues->operator[](i) = valuesVariables[index];
                    //LOG_DBG("New values %s:%s for variable with index=: %d",newValues->operator[](i) ? "true" : "false",valuesVariables[index]? "true" : "false",index);
                    i++;    
                }
            }
        }
        else
        {
            if (newGain < currentGain)
            {
                LOG_DBG("New gain min: %ld",newGain);
                currentGain = newGain;
                i = 0;
                for (auto &index : indexOfVariablesHandled)
                {
                    newValues->operator[](i) = valuesVariables[index];
                    i++;    
                }                
            }    
        }               

    }

    return currentGain;    
    
}

std::unordered_map<std::string, double> MGM::getUtilMsgToParent(std::vector<std::string> constraints_shared_in,std::vector<std::string> *constraints_shared_out)
{
    std::vector<bool> valuesVariables = this->valuesVariables;
    std::vector<bool> currentValues;
    std::vector<bool> parentValues;

    //Results for each cycle
    std::vector<bool> bestCurrentValues;
    std::vector<bool> bestParentValues;

    //Table with the Util values to return
    std::unordered_map<std::string, double> utilTable;

    int i = 0;

    //Obtain the shared constraints with parents and pseudo parents and with this node
    std::unordered_map<std::string, std::string> constraints_shared;

    for (auto constr : this->query_map_constraint)
    {
        for (auto &index : indexOfVariablesHandledByParentsAndPseudoParents)
        {
            if (constr.second.find("x" + std::to_string(index)) != std::string::npos && find(constraints_shared_in.begin(), constraints_shared_in.end(), constr.first) == constraints_shared_in.end())
                constraints_shared.insert({constr.first,constr.second});  
        }

        for (auto &index : indexOfVariablesHandled)
        {
            if (constr.second.find("x" + std::to_string(index)) != std::string::npos && find(constraints_shared_in.begin(), constraints_shared_in.end(), constr.first) == constraints_shared_in.end())
                constraints_shared.insert({constr.first,constr.second});  
        }        
    }

    for (auto constr : constraints_shared)
        constraints_shared_in.push_back(constr.first);

    *constraints_shared_out = constraints_shared_in;
    

    //Print the shared constraint
    std::string result;

    for (const auto& pair : constraints_shared) {
        result += pair.first + " ";
    }

    // Rimuove l'ultimo spazio (opzionale)
    if (!result.empty()) {
        result.pop_back();
    }

    LOG_DBG("Shared constraints: %s",result.c_str());

    for (auto &index : indexOfVariablesHandledByParentsAndPseudoParents)
    {
        parentValues.push_back(false);
        bestParentValues.push_back(false);
        valuesVariables[index] = false;
        i++;
    } 

    i = 0;
    for (auto &index : indexOfVariablesHandled)
    {
        currentValues.push_back(false);
        bestCurrentValues.push_back(false);
        valuesVariables[index] = false;
        i++;
    }


    double currentGain;
    bool first = true; 

    //for loop to handle the values of the 
    for (int j = 0; j < pow(2,parentValues.size()); j++)
    {
        for (int k = 0; k < parentValues.size(); k++)
        {
            parentValues[k] = j & (1<< k);
        }

        i=0;

        for (auto &index : indexOfVariablesHandledByParentsAndPseudoParents)
        {
            valuesVariables[index] = parentValues[i];
            i++;
        }
        first = true;
        for (int z = 0; z < pow(2,currentValues.size()); z++)
        {
            for (int n = 0; n < currentValues.size(); n++)
            {
                currentValues[n] = z & (1<< n);
            }

            i=0;

            for (auto &index : indexOfVariablesHandled)
            {
                valuesVariables[index] = currentValues[i];
                i++;
            }


            double newGain = 0;

            if(constraints_shared.size() != 0)
                newGain = applyConstraint(constraints_shared,valuesVariables, this->isMax);

            /* 
                Add the values of the util messages from childs and pseudo-child to the current gain
            */

            double increment = 0;

            //loop on all the table (one table for each children or pseudoChildren)
            for (auto table : this->allUtilMsgFromChild)
            {   
                //for each table
                for (auto row : table)
                {
                    auto key = row.first;
                    auto indexes = split(key,',');

                    auto size = indexes.size();

                    //All the values of the variables in the table must match with the current values
                    int matchCount = 0;

                    for (auto i : indexes)
                    {
                        auto indexAndValue = split(i,'=');

                        int index = stoi(indexAndValue[0]);

                        if(BoolToString(valuesVariables[index]) == indexAndValue[1])
                            matchCount++;
                    }
                    
                    if(matchCount == size)
                        increment+= row.second;
                }
            }
            //Apply increment of the utils msg on the new gain
            newGain+=increment;

            //built a row for the value table
            i=0;
            std::string keyV = "";
            for (auto &index : indexOfVariablesHandledByParentsAndPseudoParents)
            {
                keyV+=std::to_string(index) + "=" + BoolToString(parentValues[i]) + ",";
                i++;
            }

            i=0;

            for (auto &index : indexOfVariablesHandled)
            {
                keyV+=std::to_string(index) + "=" + BoolToString(currentValues[i]) + ",";
                i++;
            }

            if(keyV.back() == ',')
                keyV.pop_back();

            this->tableValue.insert({keyV,newGain});    
            
            
            if (first)
            {
                currentGain = newGain;
                first = false;

                i=0;
                for (auto &index : indexOfVariablesHandledByParentsAndPseudoParents)
                {
                    bestParentValues[i] = parentValues[i];
                    i++;
                }

                i=0;
                for (auto &index : indexOfVariablesHandled)
                {
                    bestCurrentValues[i] = currentValues[i];
                    i++;
                }
                
                        
            }
            else if (this->isMax)
            {
                if (newGain > currentGain)
                {   
                    //LOG_DBG("New gain max: %f",newGain);
                    currentGain = newGain;

                    i=0;
                    for (auto &index : indexOfVariablesHandledByParentsAndPseudoParents)
                    {
                        bestParentValues[i] = parentValues[i];
                        i++;
                    }

                    i=0;
                    for (auto &index : indexOfVariablesHandled)
                    {
                        bestCurrentValues[i] = currentValues[i];
                        i++;
                    }
                }
            }
            else
            {
                if (newGain < currentGain)
                {
                    //LOG_DBG("New gain min: %f",newGain);
                    currentGain = newGain;

                    i=0;
                    for (auto &index : indexOfVariablesHandledByParentsAndPseudoParents)
                    {
                        bestParentValues[i] = parentValues[i];
                        i++;
                    }

                    i=0;
                    for (auto &index : indexOfVariablesHandled)
                    {
                        bestCurrentValues[i] = currentValues[i];
                        i++;
                    }                                    
                }    
            }                           
                        
        }

        // crea una riga della matrice dove ci sono i valori delle variabili dei padri e pseudopadri e il miglior valore
        std::string key = "";

        //create a row of the Util table with the best values of the variables of the parents and pseudo-parents

        //Create the string with the values of the variables of the parents and pseudo-parents
        i=0;
        for (auto &index : indexOfVariablesHandledByParentsAndPseudoParents)
        {
            key+= std::to_string(index) + "=" + BoolToString(bestParentValues[i]) + ",";
            i++;
        }

        if(key.back() == ',')
            key.pop_back();
        
        utilTable.insert({key,currentGain});
    }

    return utilTable;
}

void MGM::dpopUtilLeaf(std::vector<std::string> constraints_shared_in)
{
    if (this->childrens.size() == this->allUtilMsgFromChild.size())
    {
        std::vector<std::string> constraints_shared_out;
        //Create Util table
        std::unordered_map<std::string, double> tableUtil = this->getUtilMsgToParent(constraints_shared_in,&constraints_shared_out);

        //Create the message to send to parent
        std::string agent;
        agent = this->parent;

        //If this is the root then no message must be sent
        if(agent == "")
            return;

        //Create the query
        request_t request;
        std::string q = "opt=UTIL&";

        for (auto constr:constraints_shared_out)
            q+= constr + "=1&";

        q.pop_back();    
        

        //Insert Table in the data of the message
        std::string dataTable = "";

        for (auto row = tableUtil.begin(); row != tableUtil.end(); row++)
        {
            dataTable+= row->first + ":" + std::to_string(row->second) + "$";
        }
            
        if(dataTable.back() == '$');
            dataTable.pop_back();

        //Create the request and receive the response
        std::vector<std::string> ipAndPort = split(agent, '@');

        request.method = POST;
        request.path = "MGM";
        request.query = q.c_str();
        request.dst_host = ipAndPort[0].c_str();
        request.dst_port = ipAndPort[1].c_str();
        request.data = reinterpret_cast<const uint8_t *>(dataTable.c_str());
        request.data_length = dataTable.length();

        LOG_DBG("Send Message to: %s@%s with query: %s", ipAndPort[0].c_str(), ipAndPort[1].c_str(), q.c_str());
        LOG_DBG("data=%s",dataTable.c_str());
        message_t response = commAndRes->get_client()->send_message_and_wait_response(request);
        
    }
    
}

void MGM::dpopValue()
{
    LOG_DBG("Start Phase Value");
    bool first = true;
    std::string indexAndValuesToSend;
    double bestGain;

    LOG_DBG("allValues:");
    for (auto aIndexParent : this->allValuesMsg)
        printf("%s,",(aIndexParent.first + "=" + BoolToString(aIndexParent.second)).c_str());
        
    for (auto row : this->tableValue)
    {
        std::vector<std::string> allIndexAndValues = split(row.first,',');
        double gain = row.second;

        //If this is not the root then there are value msgs from the parent to filter
        if(this->parent != "")
        {
            int size = this->allValuesMsg.size();
            int matchCount = 0;

            for (auto aIndexParent : this->allValuesMsg)
            {
                std::string stringToMatch = aIndexParent.first + "=" + BoolToString(aIndexParent.second);

                if(std::find(allIndexAndValues.begin(),allIndexAndValues.end(),stringToMatch) != allIndexAndValues.end())
                    matchCount++;
            }
            
            if (matchCount != size)
                continue;

        }
        //Si devono aggiungere anche gli incrementi dei guadagni dei constraint non condivisi con i figli che non sono stati considerati nei nodi figli
        if (first)
        {
            bestGain = gain;
            indexAndValuesToSend = row.first;
            first = false;
        }
        else if (this->isMax && gain > bestGain)
        {
            bestGain = gain;
            indexAndValuesToSend = row.first;
        }
        else if( !this->isMax && gain < bestGain)
        {
            bestGain = gain;
            indexAndValuesToSend = row.first;
        }
    }

    LOG_DBG("\nPhase Value: selected=%s with gain=%lf",indexAndValuesToSend.c_str(),bestGain);

    //If this node is a leaf then save the best values with the best gains localy
    if (this->childrens.size() + this->pseudoChildrens.size() == 0)
    {
        std::replace( indexAndValuesToSend.begin(), indexAndValuesToSend.end(), ',', '&');
        this->bestIndexAndValues = indexAndValuesToSend;
        this->done = true;
        return;
    }    

    //Send the index and values with the best gain to the clidren and pseudo-childrens ...

    //Create the messages to send to childrens and pseudo-childrens
    //Create the query
    request_t request;
    std::string q = "opt=VALUE";

    //Insert best values of the variables and the gain in the data of the message
    std::string dataTable = "";

    dataTable+= indexAndValuesToSend + ":" + std::to_string(bestGain);
    LOG_DBG("dataTable=%s",dataTable.c_str());
    //Create the requests and receive the responses

    request.method = POST;
    request.path = "MGM";
    request.query = q.c_str();
    request.data = reinterpret_cast<const uint8_t *>(dataTable.c_str());
    request.data_length = dataTable.length();

    for (auto child : this->childrens)
    {
        std::vector<std::string> ipAndPort = split(child, '@');
        request.dst_host = ipAndPort[0].c_str();
        request.dst_port = ipAndPort[1].c_str();

        LOG_DBG("Send Message to: %s@%s with query: %s and data: %s", ipAndPort[0].c_str(), ipAndPort[1].c_str(), q.c_str(),dataTable.c_str());
        message_t response = commAndRes->get_client()->send_message_and_wait_response(request);
    }

    for (auto pseudoChild : this->pseudoChildrens)
    {
        std::vector<std::string> ipAndPort = split(pseudoChild, '@');
        request.dst_host = ipAndPort[0].c_str();
        request.dst_port = ipAndPort[1].c_str();

        LOG_DBG("Send Message to: %s@%s with query: %s and data: %s", ipAndPort[0].c_str(), ipAndPort[1].c_str(), q.c_str(),dataTable.c_str());
        message_t response = commAndRes->get_client()->send_message_and_wait_response(request);        
    }
    
}

MGM::~MGM()
{
}