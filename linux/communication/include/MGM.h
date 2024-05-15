#ifndef MGM_H
#define MGM_H

#include "common.h"
#include "CoapServer.h"
#include "eddie.h"

#include <coap3/coap.h>
#include <map>

class MGM
{
private:
    coap_context_t *context;
    CoapServer *commAndRes;

    std::vector<Link> variablesHandled;
    std::vector<int> indexOfVariablesHandled;

    std::vector<std::string> allVariables;
    std::vector<bool> valuesVariables;

    //For the timer
    struct timeval tv;
    struct timezone tz;

    //Threads
    std::thread mgm_thread;

    bool quit = false;

    static void
    MGM_post(coap_resource_t *resource, coap_session_t *session, const coap_pdu_t *request,
                const coap_string_t *query, coap_pdu_t *response);
    static void
    MGM_get(coap_resource_t *resource, coap_session_t *session, const coap_pdu_t *request,
                const coap_string_t *query, coap_pdu_t *response);

    static void
    MGM_put(coap_resource_t *resource, coap_session_t *session, const coap_pdu_t *request,
                const coap_string_t *query, coap_pdu_t *response);
    
    static void
    MGM_delete(coap_resource_t *resource, coap_session_t *session, const coap_pdu_t *request,
                const coap_string_t *query, coap_pdu_t *response);                                                

public:
    MGM(coap_context_t *server_context,CoapServer *server);
    ~MGM();
    void executeAlgo();
    void mgmAlgo();
};


#endif 