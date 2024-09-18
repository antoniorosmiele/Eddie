/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileCopyrightText: Huawei Inc.
 * SPDX-FileCopyrightText: Politecnico Di Milano
 */
#include "EddieEndpoint.h"
#include "common.h"

#define DEFAULT_LT 90000
#define NOW std::chrono::system_clock::now()

static ResourceDirectory *resource_directory = nullptr;

CoapClient* EddieEndpoint::get_client(){
    return this->server ? this->server->get_client() : nullptr;
}

CoapServer* EddieEndpoint::get_server() {
    return this->server;
}

std::string EddieEndpoint::get_resource_dir_ip() {
    return this->resource_directory_ip;
}

std::string EddieEndpoint::get_resource_dir_port() {
    return this->resource_directory_port;
}

EddieEndpoint::EddieEndpoint(const std::string& port, const std::string& ip) {
    server = new CoapServer(ip.empty() ? get_local_node_ip().c_str() : ip.c_str(),
                            port.c_str());

    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    std::ostringstream oss;
    oss << now;
    ep = oss.str();
    d = ep;
}

EddieEndpoint::~EddieEndpoint() {
    unpublish_resources();
    delete resource_directory;
    stop_server();
    delete server;
    coap_cleanup();
}

int EddieEndpoint::discover_rd() {
    LOG_DBG("Discovering resource directory");

    const char* multicast_addr = get_server()->is_ipv6() ? COAP_IP6_MULTICAST_SITE_LOCAL : COAP_IP4_MULTICAST;

    request_t request {
            multicast_addr,
            "5683",
            GET,
            ".well-known/core",
            "rt=core.rd",
            nullptr,
            0,
            false,
            40
    };

    message_t response = get_client()->send_message_and_wait_response(request,10);

    std::vector<Link> parsed = parse_link_format(response.data);

    if (parsed.empty()) return -1;

    resource_directory_ip = response.src_host;
    resource_directory_port = response.src_port;

    LOG_DBG("Found resource directory: ip=%s port=%s",
            this->resource_directory_ip.c_str(), this->resource_directory_port.c_str());

    return 0;
}

std::vector<Link> EddieEndpoint::get_resources_from_rd() {
    if (resource_directory_ip.empty() || resource_directory_port.empty()) {
        LOG_ERR("unknown resource directory address, please discover it first or initialize one");
        return {};
    }

    request_t request{
            resource_directory_ip.c_str(),
            resource_directory_port.c_str(),
            GET,
            "rd-lookup/res",
            nullptr,
            nullptr,
            0,
            true,
            40
    };
    message_t response = get_client()->send_message_and_wait_response(request);

    if (response.status_code != CONTENT) {
        LOG_ERR("Error getting resources from rd: status_code=%d, content=%s", response.status_code, response.data.c_str());
        return {};
    }
    return parse_link_format(response.data);
}

int EddieEndpoint::add_resource(EddieResource *resource) {
    LOG_DBG("Adding resource: %s", resource->get_path_str().c_str());
    return get_server()->add_resource(resource);
}

int EddieEndpoint::start_server(bool blocking) {
    LOG_DBG("starting server");
    if (blocking)
        return get_server()->run();
    else
        return get_server()->start_server();
}

int EddieEndpoint::stop_server() {
    LOG_DBG("stopping server");
    return get_server()->stop_server();
}

int EddieEndpoint::publish_resources() {
    LOG_DBG("Publishing resources");

    if (resource_directory_ip.empty() || resource_directory_port.empty()) {
        LOG_ERR("unknown resource directory address, please discover it first or initialize one");
        return -1;
    }

    std::string base_address = ip_to_url(get_server()->get_my_ip(), get_server()->get_my_port());

    std::string data = CoapServer::get_resources_in_linkformat();

    if (data.empty()) {
        LOG_DBG("[CoapServer::publish_resources]: unable to get resources in link format");
        return -1;
    }

    std::string query = "ep=" + ep + "&d=" + d + "&lt=" + std::to_string(DEFAULT_LT);
    if (!base_address.empty()) query +=  + "&base=" + base_address;
    query = url_encode(query);

    request_t request {
            resource_directory_ip.c_str(),
            resource_directory_port.c_str(),
            POST,
            "rd",
            query.c_str(),
            reinterpret_cast<const uint8_t *>(data.c_str()),
            data.length(),
            true,
            40
    };

    message_t response = get_client()->send_message_and_wait_response(request);
    if (!response.location_path.empty()) {
        this->rd_endpoint = "rd/" + response.location_path;
    }
    else {
        LOG_ERR("Location path option not found in response");
        return -1;
    }

    return 0;
}

int EddieEndpoint::unpublish_resources() {
    if (rd_endpoint.empty()) {
        LOG_ERR("rd_endpoint is empty, nothing to unpublish");
        return -1;
    }

    request_t request {
            resource_directory_ip.c_str(),
            resource_directory_port.c_str(),
            DELETE,
            this->rd_endpoint.c_str(),
            nullptr,
            nullptr,
            0,
            true,
            40
    };
    message_t response = get_client()->send_message_and_wait_response(request);
    if (response.status_code != DELETED) return -1;

    return 0;
}

void EddieEndpoint::init_resource_dir_local() {
    LOG_DBG("Initializing local resource directory");
    if (resource_directory) {
        LOG_ERR("local resource directory already initialized");
        return;
    }
    resource_directory = new ResourceDirectory(get_server()->get_my_ip(), get_server()->get_context());
    resource_directory_ip = get_server()->is_ipv6() ? "::1" : "127.0.0.1";
    resource_directory_port = get_server()->get_my_port();
}
