/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileCopyrightText: Huawei Inc.
 * SPDX-FileCopyrightText: Politecnico Di Milano
 */

#ifndef RESOURCEDIRECTORY_H
#define RESOURCEDIRECTORY_H

#include "common.h"

#include <coap3/coap.h>
#include <map>

/**
 * This is an implementation of the Constrained RESTful Environments (CoRE) Resource Directory
 * as described in [rfc9176](https://datatracker.ietf.org/doc/html/rfc9176)
 */
class ResourceDirectory {
private:
    static ThreadSafeMap<std::string, coap_resource_t *> *rd_endpoints;
    static ThreadSafeMap<std::string, coap_resource_t *> *rd_endpoints_by_ep_d;

    coap_context_t *context;

    bool quit = false;

    static void
    hnd_post_rd(coap_resource_t *resource, coap_session_t *session, const coap_pdu_t *request,
                const coap_string_t *query, coap_pdu_t *response);

    static void
    hnd_get_lookup_res(coap_resource_t *resource, coap_session_t *session, const coap_pdu_t *request,
                       const coap_string_t *query, coap_pdu_t *response);

    static void
    hnd_get_lookup_ep(coap_resource_t *resource, coap_session_t *session, const coap_pdu_t *request,
                      const coap_string_t *query, coap_pdu_t *response);

    static void
    hnd_post_rd_endpoint(coap_resource_t *resource, coap_session_t *session, const coap_pdu_t *request,
                         const coap_string_t *query, coap_pdu_t *response);

    static void
    hnd_delete_rd_endpoint(coap_resource_t *resource, coap_session_t *session, const coap_pdu_t *request,
                           const coap_string_t *query, coap_pdu_t *response);

    static coap_resource_t* create_endpoint(
        const std::unordered_map<std::string, std::string> &params, 
        const std::string &data
    );

    static std::string add_endpoint_to_rd(const std::string& ep_d_key,
                                    coap_context_t *context,
                                    const std::unordered_map<std::string, std::string> &params, 
                                    const std::string &data);

    /**
     * Get the list of endpoints potentially filtered by attributes
     * @param filters The endpoints that match the attributes listed in this map will be returned.
     * If filters is empty then no filtering will be applied.
     * @return List of endpoints, each endpoint is described in link format.
     */
    static std::list<std::string>
    endpoints_to_string_list_filtered(std::map<std::string, std::string> filters);

    /**
     * Get the list of resources potentially filtered by attributes
     * @param filters The resources that match the attributes listed in this map will be returned.
     * If filters is empty then no filtering will be applied.
     * @return List of resources, each resource is described in link format.
     */
    static std::list<std::string>
    rd_resources_to_string_list_filtered(std::map<std::string, std::string> filters);

    int init_server_context();

    [[nodiscard]] bool get_quit() const {
        return quit;
    }

    void set_quit(bool val) {
        quit = val;
    }

public:

    /**
     * @brief Construct and initialize a new Resource Directory 
     * 
     * @param server_context A preinitialized server context. If not specified, a new context will be created.
     */
    explicit ResourceDirectory(const std::string& ip, coap_context_t *server_context = nullptr);

    /**
     * Stop resource directory and releases resources
     */
    ~ResourceDirectory();

    /**
     * Run the resource directory I/O loop until the resource directory is destroyed
     * @return 0 on success
     */
    int run();
};

#endif 