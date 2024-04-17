/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileCopyrightText: Huawei Inc.
 * SPDX-FileCopyrightText: Politecnico Di Milano
 */

#include "common.h"
#include "eddie.h"

#include <coap3/coap.h>
#include <netdb.h>
#include <cstdio>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <string>

int resolve_address(const char *host, const char *service, coap_address_t *dst) {
    struct addrinfo *res, *aInfo;
    struct addrinfo hints{};
    int error, len = -1;

    memset(&hints, 0, sizeof(hints));
    memset(dst, 0, sizeof(*dst));
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_family = AF_UNSPEC;

    error = getaddrinfo(host, service, &hints, &res);

    if (error != 0) {
        LOG_ERR("getaddrinfo: %s", gai_strerror(error));
        return error;
    }

    for (aInfo = res; aInfo != nullptr; aInfo = aInfo->ai_next) {
        switch (aInfo->ai_family) {
            case AF_INET6:
            case AF_INET:
                len = dst->size = aInfo->ai_addrlen;
                memcpy(&dst->addr.sin6, aInfo->ai_addr, dst->size);
                freeaddrinfo(res);
                return len;
            default:;
        }
    }

    freeaddrinfo(res);
    return len;
}

bool match(const std::string &src, const std::string &dst, int is_prefix) {
    std::regex re;

    if (is_prefix) {
        re = std::regex("^" + dst + ".*");
    } else {
        re = std::regex("^" + dst);
    }

    return std::regex_match(src, re);
}

std::vector<std::string> split(const std::string &s, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);

    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }

    return tokens;
}

std::string get_local_node_ip() {
    std::string ip;
    struct ifaddrs *if_addresses;
    int rc;
    char host[NI_MAXHOST];

    if (getifaddrs(&if_addresses) == -1) {
        perror("[CoapNode::connect]: getifaddrs");
        return "";
    }

    for (struct ifaddrs *ifa = if_addresses; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET6 && strcmp(ifa->ifa_name, "lo") != 0) {
            rc = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in6), host, NI_MAXHOST,
                             nullptr, 0, NI_NUMERICHOST);

            if (rc != 0) {
                LOG_ERR("[CoapNode::connect]: getnameinfo failed: %s", gai_strerror(rc));
                continue;
            }

            ip = host;
            break;
        }
    }

    if (ip.empty()) {
        LOG_ERR("[CoapNode::connect]: no valid ip address found for this machine");
    }

    return ip;
}

method_t coap_method_to_method(coap_pdu_code_t method) {
    switch (method) {
        case COAP_REQUEST_CODE_GET:
            return GET;
        case COAP_REQUEST_CODE_POST:
            return POST;
        case COAP_REQUEST_CODE_PUT:
            return PUT;
        case COAP_REQUEST_CODE_DELETE:
            return DELETE;
        default:
            LOG_ERR("Invalid coap method");
            return GET;
    }
}

coap_pdu_code_t method_to_coap_method(method_t method) {
    switch (method) {
        case GET:
            return COAP_REQUEST_CODE_GET;
        case POST:
            return COAP_REQUEST_CODE_POST;
        case PUT:
            return COAP_REQUEST_CODE_PUT;
        case DELETE:
            return COAP_REQUEST_CODE_DELETE;
        default:
            LOG_ERR("Invalid coap method");
            return COAP_REQUEST_CODE_GET;
    }
}

std::string ip_to_url(const std::string& ip, const std::string& port) {
    struct addrinfo *result;
    int ret = getaddrinfo(ip.c_str(), nullptr, nullptr, &result);
    if (ret != 0) {
        LOG_ERR("getaddrinfo error");
        return "";
    }
    switch (result->ai_family) {
        case AF_INET:
            return "coap://" + ip + ":" + port;
        case AF_INET6:
            return "coap://[" + ip + "]:" + port;
        default:
            LOG_ERR("Invalid ip address");
            return "";
    }
}

std::string url_encode(const std::string &value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (std::string::const_iterator i = value.begin(), n = value.end(); i != n; ++i) {
        std::string::value_type c = (*i);

        // Keep alphanumeric and other accepted characters intact
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
            continue;
        }

        // Any other characters are percent-encoded
        escaped << std::uppercase;
        escaped << '%' << std::setw(2) << int((unsigned char) c);
        escaped << std::nouppercase;
    }

    return escaped.str();
}