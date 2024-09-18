/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileCopyrightText: Huawei Inc.
 * SPDX-FileCopyrightText: Politecnico Di Milano
 */

#include "CoapServer.h"
#include "common.h"

#include <coap3/coap.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <chrono>
#include <string>

unsigned char *g_buf = nullptr;

class CoapClient;

ThreadSafeMap<std::string, coap_resource_t *> *CoapServer::resources = new ThreadSafeMap<std::string, coap_resource_t *>();

static std::string resource_list_to_string(const std::list<coap_resource_t *> &resources) {
    size_t buf_size = 1000;
    auto *buf = static_cast<unsigned char *>(malloc(buf_size * sizeof(unsigned char)));
    size_t len = buf_size;
    std::string to_return;

    for (auto res: resources) {
        size_t offset = 0;
        len = buf_size;
        auto print_status = coap_print_link(res, buf, &len, &offset);

        if (print_status & COAP_PRINT_STATUS_ERROR) {
            LOG_DBG("Unable to create string representation of resource");
            free(buf);
            return "";
        }

        while (print_status & COAP_PRINT_STATUS_TRUNC) {
            buf = static_cast<unsigned char *>(realloc(buf, 2 * buf_size * sizeof(unsigned char)));
            buf_size *= 2;
            len = buf_size;
            print_status = coap_print_link(res, buf, &len, &offset);
        }

        auto res_str = std::string((char *) buf, 0, len);
        to_return += (res_str + ",");
    }

    if (!to_return.empty()) to_return.pop_back();

    free(buf);

    return to_return;
}

void CoapServer::resource_handler(coap_resource_t *resource, coap_session_t *session, const coap_pdu_t *request,
                                      const coap_string_t *query, coap_pdu_t *response) {
    // get payload
    size_t data_len, offset, total;
    const uint8_t *data;
    coap_get_data_large(request, &data_len, &data, &offset, &total);

    // get code
    coap_pdu_code_t coap_method = coap_pdu_get_code(request);

    // construct request to pass to handler
    request_t message_request;
    message_request.data = data;
    message_request.data_length = data_len;
    message_request.method = coap_method_to_method(coap_method);

    if (query != nullptr) {
        message_request.query = (const char *) query->s;
    }

    auto eddie_resource = static_cast<EddieResource *>(coap_resource_get_userdata(resource));
    message_t response_message = eddie_resource->request_handler(message_request);

    coap_pdu_set_code(response, static_cast<coap_pdu_code_t>(response_message.status_code));

    if (g_buf != nullptr) free(g_buf);
    g_buf = static_cast<unsigned char *>(malloc(sizeof(unsigned char) * response_message.data.length() + 1));
    strcpy((char *) g_buf, response_message.data.c_str());
    auto len = response_message.data.size();

    if (!response_message.data.empty()) {
        coap_add_data_large_response(resource,
                                     session,
                                     request,
                                     response,
                                     query,
                                     COAP_MEDIATYPE_APPLICATION_JSON,
                                     1, 0,
                                     len,
                                     g_buf,
                                     nullptr, nullptr);
    }
}

CoapServer::CoapServer(const char *host, const char *port) 
{
    my_ip = host;
    my_port = port;

    LOG_DBG("Initializing CoapServer ip=%s port=%s", my_ip.c_str(), my_port.c_str());
    init_server(is_ipv6() ? "::" : "0.0.0.0", port);
    this->client = new CoapClient();

    std::vector<std::string> ipAndInterface;

    if (my_ip.find('%') !=  std::string::npos)
    {
        ipAndInterface = split(my_ip,'%');
        my_interface = ipAndInterface[1];
    }
    else
    {
        my_interface = "";
    }
    
    this->client->set_interface(my_interface);
    LOG_DBG("Saved interface: %s",my_interface.c_str());
    LOG_DBG("Initialized CoapServer");

}

CoapServer::~CoapServer() {
    stop_server();
    resources->delete_all();
    delete resources;
    delete client;
    coap_free_context(this->context);
    if (g_buf != nullptr) free(g_buf);
    delete this->mgm;
}

bool CoapServer::get_quit() const {
    return quit;
}

void CoapServer::set_quit(bool val) {
    quit = val;
}

CoapClient *CoapServer::get_client() {
    return client;
}

int CoapServer::init_server(const char *host, const char *port) {
    coap_address_t dst;

    if (!port) {
        port = "5683";
    }

    coap_startup();

    if (resolve_address(host, port, &dst) < 0) {
        LOG_ERR("Failed to resolve address");
        coap_free_context(this->context);
        coap_cleanup();
        return EXIT_FAILURE;
    }

    this->context = coap_new_context(nullptr);
    this->endpoint = coap_new_endpoint(context, &dst, COAP_PROTO_UDP);
    coap_context_set_block_mode(this->context, COAP_BLOCK_USE_LIBCOAP | COAP_BLOCK_SINGLE_BODY);

    if (!this->context || !this->endpoint) {
        LOG_ERR("Cannot initialize context");
        coap_free_context(this->context);
        coap_cleanup();
        return EXIT_FAILURE;
    }

    // be available on multicast groups
    coap_join_mcast_group(this->context, COAP_IP4_MULTICAST);
    coap_join_mcast_group(this->context, COAP_IP6_MULTICAST_LOCAL_LINK);
    coap_join_mcast_group(this->context, COAP_IP6_MULTICAST_SITE_LOCAL);

    //Create object MGM
    this->mgm = new MGM(this->context,this);

    return EXIT_SUCCESS;
}

int CoapServer::add_resource(EddieResource *resource) {
    auto attr_map = std::map<std::string, std::string>();

    for (const auto &attr: resource->get_attributes_str()) {
        auto attr_split = split(attr, '=');
        attr_map.insert(std::make_pair(attr_split[0], attr_split[1]));
    }

    auto lt_it = attr_map.find("lt");
    if (lt_it == attr_map.end()) {
        attr_map["lt"] = "90000";  // 25 hours
    }

    auto s = resource->get_path_str();
    coap_str_const_t* uri_path = coap_make_str_const(s.c_str());
    coap_resource_t *new_coap_resource = coap_resource_init(uri_path, 0);

    if (!new_coap_resource) {
        LOG_DBG("[CoapServer::add_resource]: Error initializing new resource");
        return -1;
    }

    coap_register_handler(new_coap_resource, COAP_REQUEST_GET, resource_handler);
    coap_register_handler(new_coap_resource, COAP_REQUEST_PUT, resource_handler);
    coap_register_handler(new_coap_resource, COAP_REQUEST_POST, resource_handler);
    coap_register_handler(new_coap_resource, COAP_REQUEST_DELETE, resource_handler);

    for (const auto &pair: attr_map) {
        coap_add_attr(new_coap_resource, coap_make_str_const(pair.first.c_str()),
                      coap_make_str_const(pair.second.c_str()), 0);
    }
    coap_resource_set_userdata(new_coap_resource, resource);

    coap_add_resource(this->context, new_coap_resource);
    CoapServer::resources->set(resource->get_path_str(), new_coap_resource);

    return 0;
}

int CoapServer::run() {
    while (!this->get_quit()) {
        coap_io_process(this->context, 250);
    }
    return 0;
}

int CoapServer::start_server() {
    auto server_task = [this]() {
        this->run();
    };
    server_thread = std::thread(server_task);
    return 0;
}

int CoapServer::stop_server() {
    set_quit(true);
    if (server_thread.joinable())
        server_thread.join();
    return 0;
}

const std::string &CoapServer::get_my_ip() const {
    return my_ip;
}

const std::string &CoapServer::get_my_port() const {
    return my_port;
}

const std::string &CoapServer::get_my_interface() const {
    return my_interface;
}

std::string CoapServer::get_resources_in_linkformat() {
    auto res_list = resources->get_all();
    return resource_list_to_string(res_list);
}

coap_context_t *CoapServer::get_context() const {
    return context;
}

bool CoapServer::is_ipv6() const {
    struct addrinfo *result;
    int ret = getaddrinfo(get_my_ip().c_str(), nullptr, nullptr, &result);
    return (ret == 0 && result->ai_family == AF_INET6);
}
