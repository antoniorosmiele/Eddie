#ifndef MGM_H
#define MGM_H

#include "common.h"
#include "CoapServer.h"
#include "eddie.h"

#include <coap3/coap.h>
#include <map>
#include <mutex>
#include <atomic>

#define SECONDS_TIMEOUT 5
#define MAX_ITERATIONS 114

class CoapServer;

class MGM
{
private:
    //Communication
    coap_context_t *context;
    CoapServer *commAndRes;
    std::string base_address;
    std::string base_port;

    //Variables of the algorithm
    std::vector<Link> variablesHandled;
    std::vector<int> indexOfVariablesHandled;

    std::vector<std::string> allVariables;
    std::vector<bool> valuesVariables;

    std::vector<std::string> allNeighbors;

    long gain;

    //For the constraints
    bool isMax;
    std::unordered_map<std::string, std::string> query_map_constraint;

    //For the values received by all the agents
    std::mutex mtx;
    std::condition_variable cv;
    std::vector<std::unordered_map<std::string, std::string>> indexAndValues_vector;
    std::vector<long> gains_vector;

    //For the timer
    struct timeval tv;
    struct timezone tz;
    long double start_time;

    //Threads
    std::thread mgm_thread;
    std::atomic<bool> done;

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
    
    //Send the current values of the variables handled to all the agents
    bool SendValueMessage(std::vector<std::string> allNeighbors, std::vector<bool> valuesVariables, std::vector<int> indexOfVariablesHandled);                                                            
    bool getValueMessages(std::vector<bool> * valuesVariables, std::vector<std::unordered_map<std::string, std::string>> indexAndValues_vector);
    long BestUnilateralGain(std::vector<bool> valuesVariables, std::vector<int> indexOfVariablesHandled,std::vector<bool>* newValues);
    bool SendGainMessage(std::vector<std::string> allNeighbors, long gain);

    double applyOp(double a, double b, char op);
    long applyConstraint(std::unordered_map<std::string, std::string> constraints,std::vector<bool> valuesVariables,bool isMax);
    double evaluateExpression(const std::string& expression, std::vector<bool> valuesVariables);
public:
    MGM(coap_context_t *server_context,CoapServer *server);
    MGM();
    ~MGM();
    void executeAlgo();
    void mgmAlgo();
    void saveSomeValuesOfVariables(std::unordered_map<std::string, std::string> values_map);
    void saveGainFromNeighbor(long gain);
    
};


#endif 