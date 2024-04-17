/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileCopyrightText: Huawei Inc.
 * SPDX-FileCopyrightText: Politecnico Di Milano
 */

#ifndef EDDIE_COAPSERVER_H
#define EDDIE_COAPSERVER_H

#include "CoapClient.h"
#include "ResourceDirectory.h"

#include <coap3/coap.h>
#include <unordered_map>
#include <netdb.h>
#include <vector>
#include <cstring>
#include <sys/ioctl.h>
#include <net/if.h>
#include <string>
#include <map>

class CoapClient;

class CoapServer {
private:
    bool quit = false;
    coap_context_t *context = nullptr;
    coap_endpoint_t *endpoint = nullptr;
    CoapClient *client;
    std::string my_ip;
    std::string my_port;

    std::thread server_thread;

    static ThreadSafeMap<std::string, coap_resource_t *> *resources;

    static void resource_handler(coap_resource_t *resource, coap_session_t *session, const coap_pdu_t *request,
                         const coap_string_t *query, coap_pdu_t *response);

    int init_server(const char *host, const char *port);

    [[nodiscard]] bool get_quit() const;

    void set_quit(bool val);

public:
    /**
     * CoapServer constructor
     * @param host The server's IP address. This IP address will used as base url in the resource directory registration
     * @param port Port number of the server as a string
     */
    explicit CoapServer(const char *host, const char *port);

    /**
     * The CoapServer destructor unpublishes the resources previously registered in the resource directory,
     * stops a potentially running server thread and releases resources.
     */
    ~CoapServer();

    /**
     * The client is the object used to send CoAP requests
     * @return The client object used by CoAP server for the communication
     */
    CoapClient *get_client();

    /**
     * Add a resource into the server. A resource consists of a path, attributes and REST methods.
     * Adding the resource allows the server to serve it and call the appropriate REST handler
     * whenever it receives a request targeting the served resource.
     * @param resource An EddieResource object containing resource information (e.g. path and attributes) and REST handlers
     * @return 0 on success, -1 on error
     */
    int add_resource(EddieResource *resource);

    /**
     * Run the server loop spinning the CoAP I/O processing function. This loop runs until set_quit(true) is called
     * @return 0 on success, negative integer on failure.
     */
    int run();

    /**
     * Start the server loop in a separate thread without blocking.
     * @return 0 on success, negative integer on failure.
     */
    int start_server();

    /**
     * Stop the server thread
     * @return 0 on success, negative integer on failure.
     */
    int stop_server();

    /**
     * @brief Get the resources added into the server in link format
     *
     * @return resources as a link format string
     */
    static std::string get_resources_in_linkformat();

    /**
     * Get IP address assigned to the server, this address is used as base
     * url in the resource directory registration
     * @return The server's IP address.
     */
    [[nodiscard]] const std::string &get_my_ip() const;

    /**
     * Get port number assigned to the server
     * @return Port number of the server as a string
     */
    [[nodiscard]] const std::string &get_my_port() const;

    /**
     * Get server context
     * @return The context of the server
     */
    [[nodiscard]] coap_context_t *get_context() const;

    /**
     * Check if the assigned IP address is IPv6 or IPv4
     * @return True if the assigned IP address is IPv6, False otherwise.
     */
    [[nodiscard]] bool is_ipv6() const;
};

#endif //EDDIE_COAPSERVER_H
