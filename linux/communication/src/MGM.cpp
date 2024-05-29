#include "MGM.h"
#include "common.h"

#include <coap3/coap.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <arpa/inet.h>
#include <map>
#include <coap3/uri.h>

void MGM::MGM_put(coap_resource_t *resource, coap_session_t *session, const coap_pdu_t *request, const coap_string_t *query, coap_pdu_t *response)
{
    //Check if the query is empty
    if (!query) 
    {
        coap_pdu_set_code(response, COAP_RESPONSE_CODE_BAD_REQUEST);
        return;
    }

    //Count the number of segment in the query
    size_t buf_len = query->length + 128;
    unsigned char buf[buf_len];
    int n_opts = coap_split_query((const uint8_t *) query->s, query->length, buf, &buf_len);

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

    unsigned char tmp_buf[64];
    char str_buff[20];

    //Obtain local address
    const coap_address_t* address = coap_session_get_addr_local(session);
    size_t length = coap_print_addr(address, tmp_buf, 64);
    std::string base_address = std::string(reinterpret_cast<const char *>(tmp_buf), length);
            
    //obtain port
    uint16_t port_int = coap_address_get_port(address);
    int len = sprintf(str_buff, "%d", port_int);

    std::string base_port = std::string(reinterpret_cast<const char *>(str_buff), len);    

    for (const auto &token: sub_queries) 
    {
        std::vector<std::string> sub_query = split(token, '=');
    
        if(sub_query[0].compare("id") == 0)
            ids.emplace_back(sub_query[1]); //For the id of resource ("id=allarm")
        else if (sub_query[0].compare("neigh") == 0) // For the ip and port of the agents (neigh=ip:port)
        {
            std::vector<std::string> ipAndPort = split(token, ':');

            if (ipAndPort[0].compare(base_address) != 0 || ipAndPort[1].compare(base_port) != 0)
            {
                neighbors.emplace_back(ipAndPort[0] + ":" + ipAndPort[1]);
            }
            
        }
        else if (sub_query[0].compare("max/min") == 0) //For the type of optimisation (max/min=type)
        {
            if (sub_query[1].compare(MGM_MAX) == 0)
                maxOrMin =true;
            else
                maxOrMin = false;
        }
        else
            query_map_constraint.insert(std::make_pair(sub_query[0], sub_query[1]));  // For the constraint ("Con_name=exp")   
    }

    //If there aren't no ids of the variables then send an error message
    if(ids.size() == 0)
    {
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
        std::string id_local = link.attrs.find("id")->second;
        
         std::vector<std::string>::iterator elem = std::find(ids.begin(), ids.end(), id_local);

        if ( elem != ids.end() )
        {   int index = elem - ids.begin();
            mgm->indexOfVariablesHandled.emplace_back(index);
            mgm->variablesHandled.emplace_back(link);
        }    

    }

    //Save in an Array inside MGM the ids of the variables, initialise the value to false and save the ips and ports of the Agents

    for (const auto &id: ids)
    {
        mgm->allVariables.emplace_back(id);
        mgm->valuesVariables.emplace_back(false);
    }

    copy(mgm->allNeighbors.begin(), mgm->allNeighbors.end(), back_inserter(mgm->allNeighbors));

    //Save local ip and port
    mgm->base_address = base_address;
    mgm->base_port = base_port;

    //Save the constraints ...
    mgm->isMax = maxOrMin;
    //........................

    //Modify response message...

    coap_pdu_set_code(response, COAP_RESPONSE_CODE_CREATED);
}

void MGM::MGM_post(coap_resource_t *resource, coap_session_t *session, const coap_pdu_t *request, const coap_string_t *query, coap_pdu_t *response)
{
    //Check if the query is empty
    if (!query) 
    {
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
        mgm->executeAlgo();

        coap_pdu_set_code(response, COAP_RESPONSE_CODE_CREATED);
        return;        
    }
    else if (opt->second == "UPDATE")//If opt is the command UPDATE then save the new values of the variables in the class MGM
    {
        query_map.erase("opt");

        //Need a mutex
        {
            mgm->saveSomeValuesOfVariables(query_map);
        }
        coap_pdu_set_code(response, COAP_RESPONSE_CODE_CREATED);
        return;
    }
    else if (opt->second == "UPDATEG")
    {
        auto gainStr = query_map.find("gain");
        
        if (gainStr == query_map.end()) 
        {
            coap_pdu_set_code(response, COAP_RESPONSE_CODE_BAD_REQUEST);
            return;
        }

        {
            mgm->saveGainFromNeighbor(stol(gainStr->second));
        }

        coap_pdu_set_code(response, COAP_RESPONSE_CODE_CREATED);
        return;

    }
    else
    {
        coap_pdu_set_code(response, COAP_RESPONSE_CODE_BAD_REQUEST);
        return;
    }
    
    
    
    

}

void MGM::MGM_get(coap_resource_t *resource, coap_session_t *session, const coap_pdu_t *request, const coap_string_t *query, coap_pdu_t *response)
{

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
}

void MGM::mgmAlgo()
{

    for (size_t i = 0; i < this->variablesHandled.size(); i++)
    {
        int index = this->indexOfVariablesHandled[i];
        this->valuesVariables[index] = rand()%2;
    }

    gettimeofday(&tv, &tz);
    start_time = tv.tv_sec + 0.000001*tv.tv_usec;

    long double curr_time = start_time;

    while (curr_time- start_time > SECONDS_TIMEOUT)
    {
        //SendValueMessage(allNeighbors, currentValue)
        bool status = SendValueMessage(allNeighbors,valuesVariables,indexOfVariablesHandled);

        bool done = false;

        //currentContext = getValueMessages(allNeighbor)
        while (!done)
        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock,[&](){return indexAndValues_vector.size() == allNeighbors.size();});
            getValueMessages(&valuesVariables,indexAndValues_vector);
            done = true;
            cv.notify_all();
        }

        this->indexAndValues_vector.clear();
        
        //[gain,newValue] = BestUnilateralGain(currentContext)
        std::vector<bool> newValues = {};

        newValues.reserve(indexOfVariablesHandled.size());
        
        long gain = BestUnilateralGain(valuesVariables,indexOfVariablesHandled, &newValues);

        //SendGainMessage(allNeighbors, gain)
        status = SendGainMessage(allNeighbors,gain);

        //neighborsGains = ReceiveGainMessages(allNeighbors)
        done = false;

        while (!done)
        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock,[&](){return gains_vector.size() == allNeighbors.size();});
            
            done = true;
            cv.notify_all();
        }
        
        if (gain > *std::max_element(gains_vector.begin(), gains_vector.end()))
        {
            for (auto &index : this->indexOfVariablesHandled)
            {
                int i = 0;
                this->valuesVariables[index] = newValues[i];
                i++;
            }
            

        } 
        //  currentValue = newValue
        //end if
        gettimeofday(&tv, &tz);
        curr_time = tv.tv_sec + 0.000001*tv.tv_usec;
    }
        
}
inline const std::string  BoolToString(bool b)
{
  return b ? "true" : "false";
}

inline const bool  StringToBool(std::string s)
{
  return (s == "true") ? true : false;
}

void MGM::saveSomeValuesOfVariables(std::unordered_map<std::string, std::string> values_map)
{
    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock,[&](){return indexAndValues_vector.size()< allNeighbors.size();});
        this->indexAndValues_vector.push_back(values_map);
        cv.notify_all();
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
        std::string q = "opt=UPDATEG&gain=" + gain;

        //Create the request and receive the response
        std::vector<std::string> ipAndPort = split(agent, ':');

        request.method = POST;
        request.path = "MGM";
        request.query = q.c_str();
        request.dst_host = ipAndPort[0].c_str();
        request.dst_port = ipAndPort[1].c_str();

        message_t response = commAndRes->get_client()->send_message_and_wait_response(request);

        //Handle the response
        
        if (response.status_code == COAP_RESPONSE_CODE_BAD_REQUEST)
        {
            return false;
        }
        else
        {
            return true;
        }      
    }    
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
            q+= *iter + "=" + BoolToString(valuesVariables[*iter]) + "&"; // "opt=UPDATE&3=true&7=false&11=true"
        }
        
        if(q.back() == '&')
               q.pop_back();

        //Create the request and receive the response
        std::vector<std::string> ipAndPort = split(agent, ':');

        request.method = POST;
        request.path = "MGM";
        request.query = q.c_str();
        request.dst_host = ipAndPort[0].c_str();
        request.dst_port = ipAndPort[1].c_str();

        message_t response = commAndRes->get_client()->send_message_and_wait_response(request);

        //Handle the response
        
        if (response.status_code == COAP_RESPONSE_CODE_BAD_REQUEST)
        {
            return false;
        }
        else
        {
            return true;
        }      
    }
}

bool MGM::getValueMessages(std::vector<bool> * valuesVariables, std::vector<std::unordered_map<std::string, std::string>> indexAndValues_vector)
{
    for (auto &map : indexAndValues_vector)
    {
        for(std::unordered_map<std::string, std::string>::iterator it = map.begin(); it != map.end(); ++it) 
        {
            (*valuesVariables)[stoi(it->first)] = StringToBool(it->second);
        }
    }

    return true;
    
}

long MGM::BestUnilateralGain(std::vector<bool> valuesVariables, std::vector<int> indexOfVariablesHandled, std::vector<bool>* newValues)
{
    long currentGain = gain;

    std::vector<bool> currentValues;
    currentValues.reserve(indexOfVariablesHandled.size());

    int i = 0;

    for (auto &index : indexOfVariablesHandled)
    {
        currentValues[i] = false;
        i++;
    }

    //    
    do
    {
        i=0;

        for (auto &index : indexOfVariablesHandled)
        {
            valuesVariables[index] = currentValues[i];
            i++;
        }

        long newGain;
        //newGain = applyConstraint(this->constraint,valuesVariables)

        if (this->isMax)
        {
            if (newGain > currentGain)
            {    
                currentGain = newGain;
                i = 0;
                for (auto &index : indexOfVariablesHandled)
                {
                    (*newValues)[i] = valuesVariables[index];
                    i++;    
                }
            }
        }
        else
        {
            if (newGain < currentGain)
            {
                currentGain = newGain;
                i = 0;
                for (auto &index : indexOfVariablesHandled)
                {
                    (*newValues)[i] = valuesVariables[index];
                    i++;    
                }                
            }    
        }               

    } while (std::next_permutation(currentValues.begin(),currentValues.end()));
    
    
}

MGM::~MGM()
{
}