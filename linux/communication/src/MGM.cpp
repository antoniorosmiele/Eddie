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

    for (const auto &token: sub_queries) 
    {
        std::vector<std::string> sub_query = split(token, '=');
    
        if(sub_query[0].compare("id") == 0)
            ids.emplace_back(sub_query[1]); //For the id of resource ("id=allarm")
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

    //Save in an Array inside MGM the ids of the variables ad initialise the value to false

    for (const auto &id: ids)
    {
        mgm->allVariables.emplace_back(id);
        mgm->valuesVariables.emplace_back(false);
    }

    //Save the constraints ...


    //Modify response message...

    coap_pdu_set_code(response, COAP_RESPONSE_CODE_CREATED);
}

void MGM_post(coap_resource_t *resource, coap_session_t *session, const coap_pdu_t *request, const coap_string_t *query, coap_pdu_t *response)
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
}

void MGM::executeAlgo()
{
    auto mgm_task = [this]() 
    {
        this->mgmAlgo();
    };

    gettimeofday(&tv, &tz);
    
    this->mgm_thread = std::thread(mgm_task);    
}

void MGM::mgmAlgo()
{

}

MGM::~MGM()
{
}