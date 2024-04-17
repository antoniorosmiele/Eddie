/*
 * Copyright (c) 2022 Huawei Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __EDDIEENDPOINT_H__
#define __EDDIEENDPOINT_H__

#include "CoapClient.h"
#include "CoapServer.h"
#include "eddie.h"

#include <string>

class EddieEndpoint {
private:
    CoapServer *server = nullptr;

    std::string resource_directory_ip;
    std::string resource_directory_port;

    std::string ep;                 // endpoint name
    std::string d;                  // sector name
    std::string rd_endpoint;        // rd endpoint string

public:

    /**
     * EddieEndpoint constructor
     * @param port Port number to assign to the node
     * @param ip IP address that is assigned to the node, this IP address will be the same used to
     * register the node's resources into the resource directory.
     */
    explicit EddieEndpoint(const std::string& port = "5683", const std::string& ip = "");

    ~EddieEndpoint();

    /**
     * @return The client used by the node for coap communication
     */
    CoapClient* get_client();

    /**
     * @return The server that hosts and serves the added resources.
     */
    CoapServer* get_server();

    /**
     * @return The ip of the resource directory
     */
    std::string get_resource_dir_ip();

    /**
     * @return The port of the resource directory
     */
    std::string get_resource_dir_port();

    /**
     * Discover a resource directory in the network by sending a multicast CoAP request and listening for response.
     * @return 0 on success, -1 if no resource directory was found or an error occurred sending the request.
     */
    int discover_rd();

    /**
     * Get the vector of resources that are retrieved from the resource directory
     * @return Vector of Link that contains details information about the available resources in the network.
     */
    std::vector<Link> get_resources_from_rd();

    /**
     * Add a resource into the server
     * @param resource The resource containing paths, attributes and REST handlers.
     * @return 0 on success, a negative integer otherwise.
     */
    int add_resource(EddieResource *resource);

    /**
     * Start server I/O processing loop
     * @param blocking True to block until the server quits, False to run the server loop in a separate thread.
     * @return 0 on success, a negative integer otherwise.
     */
    int start_server(bool blocking = false);

    /**
     * Stop the server thread
     * @return 0 on success
     */
    int stop_server();

    /**
     * Register the resources, previously added, in the resource directory.
     * @return 0 on success, -1 on error
     */
    int publish_resources();

    /**
     * Ask the RD node to remove all the resources registered by this endpoint
     * @return 0 non success, negative integer on failure
     */
    int unpublish_resources();

    /**
     * Initialize a resource directory in the current node
     */
    void init_resource_dir_local();
};

#endif