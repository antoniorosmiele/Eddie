/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileCopyrightText: Huawei Inc.
 * SPDX-FileCopyrightText: Politecnico Di Milano
 */

#include "CoapClient.h"
#include "common.h"

#include <coap3/coap.h>
#include <cstdio>
#include <cstring>
#include <arpa/inet.h>
#include <netdb.h>

#define COAP_DEFAULT_LEISURE_UNSIGNED 5
#define NOW std::chrono::system_clock::now()

/** Container for all response received from the various CoAP nodes */
ThreadSafeMap<std::string, message_t> *CoapClient::messages = new ThreadSafeMap<std::string, message_t>();

std::mutex CoapClient::client_mutex;
std::condition_variable CoapClient::client_condition;

coap_response_t
CoapClient::message_handler(coap_session_t *session, const coap_pdu_t *sent, const coap_pdu_t *received,
                            const coap_mid_t id) {

    auto token = coap_pdu_get_token(received);
    std::string token_str = std::string(reinterpret_cast<const char *>(token.s), token.length);

    size_t len, offset, total;
    const uint8_t* data;
    coap_get_data_large(received, &len, &data, &offset, &total);
    coap_pdu_code_t code = coap_pdu_get_code(received);

    if (len != total) {
        LOG_DBG("Received incomplete data with offset %zu, length %zu", offset, len);
        LOG_DBG("Data: %.*s", (int) len, data);

        return COAP_RESPONSE_OK;
    }

    coap_opt_iterator_t opt_iter;
    coap_opt_t *option = coap_check_option(received, COAP_OPTION_LOCATION_PATH, &opt_iter);
    std::string location_path;
    if (option) {
        option = coap_option_next(&opt_iter);
        location_path = std::string((const char *) coap_opt_value(option), coap_opt_length(option));
    }

    const coap_address_t* remote = coap_session_get_addr_remote(session);
    char host[NI_MAXHOST];
    char service[8];
    int rc = getnameinfo(&remote->addr.sa, remote->size, host, NI_MAXHOST,
                         service, 8, NI_NUMERICHOST);

    if (rc != 0) {
        LOG_ERR("[CoapNode::connect]: getnameinfo failed: %s", gai_strerror(rc));
        host[0] = '\0';
        service[0] = '\0';
    }

    message_t response_message {
        std::string(reinterpret_cast<const char *>(data), len),
        static_cast<response_code_t>(code),
        std::string(reinterpret_cast<const char *>(host), strlen(host)),
        std::string(reinterpret_cast<const char *>(service), strlen(service)),
        location_path
    };

    messages->set(token_str, response_message);

    CoapClient::client_condition.notify_one();
    return COAP_RESPONSE_OK;
}

coap_session_t *
CoapClient::open_session(const char *dst_host, const char *dst_port) {
    coap_session_t *session;
    coap_address_t dst;
    //Piuttosto che fare il resolve address ogni volta , si salvano i coap_address risultanti in una tabella hash
    if (resolve_address(dst_host, dst_port, &dst) < 0) {
        LOG_ERR("Unable to open connection");
        return nullptr;
    }

    //LOG_DBG("Resolved addr:%s" , dst.addr.sin6);
    session = coap_new_client_session(this->context, nullptr, &dst, COAP_PROTO_UDP);

    if (!session) {
        LOG_ERR("Unable to initiate session");
        coap_session_release(session);
        return nullptr;
    }

    return session;
}

CoapClient::CoapClient(coap_context_t *context) {
    this->context = context ? context : coap_new_context(nullptr);

    if (!this->context) {
        LOG_ERR("Unable to create context");
        // TODO: throw exception
        return;
    }

    coap_context_set_block_mode(this->context, COAP_BLOCK_USE_LIBCOAP | COAP_BLOCK_SINGLE_BODY);

    coap_register_response_handler(this->context, message_handler);

    auto client_run = [this]() {
        while (!this->get_quit())
            coap_io_process(this->context, 100);
        LOG_DBG("terminating CoapClient thread");
    };
    client_thread = std::thread(client_run);
}

CoapClient::~CoapClient() {
    set_quit(true);
    client_thread.join();

    auto coap_messages = messages->get_all();
    messages->delete_all();
    delete messages;

    coap_free_context(this->context);
    coap_cleanup();
}

bool CoapClient::get_quit() const {
    return this->quit;
}

void CoapClient::set_quit(bool value) {
    this->quit = value;
}

std::string CoapClient::send_message(request_t request) {
    uint8_t buf[1024];
    uint8_t *sbuf = buf;
    coap_pdu_t *pdu;
    int res;
    coap_optlist_t *optlist = nullptr;

    // BEGIN CRITICAL CALLS TO CoAP APIs
    std::unique_lock<decltype(CoapClient::client_mutex)> lock(CoapClient::client_mutex);

    //LOG_ERR("Opening session host=%s,port=%s",request.dst_host,request.dst_port);
    coap_session_t * session = open_session(request.dst_host, request.dst_port);
    if (!session) {
        LOG_ERR("Error creating remote session");
        coap_session_release(session);
        return "";
    }

    // Initialize the PDU to send
    pdu = coap_pdu_init(request.confirmable ? COAP_MESSAGE_CON : COAP_MESSAGE_NON,
                        method_to_coap_method(request.method),
                        coap_new_message_id(session),
                        coap_session_max_pdu_size(session));

    // initialize session token to last generated token so that new sessions does not reuse same token again
    if (!last_token.empty())
        coap_session_init_token(session, last_token.length(), (const uint8_t *)last_token.c_str());

    // generate new token and add to PDU
    uint8_t token_buf[8];
    size_t token_len;
    coap_session_new_token(session, &token_len, token_buf);
    std::string token_str = std::string(reinterpret_cast<const char *>(token_buf), token_len);
    this->last_token = token_str;
    coap_add_token(pdu, token_len, token_buf);

    if (request.path) {
        size_t buflen = sizeof(buf);
        res = coap_split_path((const uint8_t *) request.path,
                              strlen(request.path),
                              sbuf,
                              &buflen);

        while (res--) {
            coap_insert_optlist(&optlist,
                                coap_new_optlist(COAP_OPTION_URI_PATH,
                                                 coap_opt_length(sbuf),
                                                 coap_opt_value(sbuf)));
            sbuf += coap_opt_size(sbuf);
        }
    } else {
        // error, destination needed
        coap_delete_pdu(pdu);
        coap_delete_optlist(optlist);
        return "";
    }

    // Add query as option to the PDU
    if (request.query) {
        size_t buflen = sizeof(buf);
        res = coap_split_query((const uint8_t *) request.query, strlen(request.query), sbuf, &buflen);

        while (res--) {
            coap_insert_optlist(&optlist,
                                coap_new_optlist(COAP_OPTION_URI_QUERY,
                                                 coap_opt_length(sbuf),
                                                 coap_opt_value(sbuf)));

            sbuf += coap_opt_size(sbuf);
        }
    }

    coap_add_optlist_pdu(pdu, &optlist);

    // check data and data type
    if (request.data && request.data_length) {
        coap_add_data_large_request(session,pdu, request.data_length, request.data,NULL,NULL);
    }

    coap_send(session, pdu);

    // END OF CRITICAL LINES
    // unlock the mutex
    lock.unlock();

    return token_str;
}

message_t CoapClient::receive_message(std::string token, int timeout) {
    message_t response;
    auto task = [&timeout, &token, &response]() {
        std::unique_lock<decltype(CoapClient::client_mutex)> lock(CoapClient::client_mutex);
        int seconds = timeout > 0 ? timeout : COAP_DEFAULT_LEISURE_UNSIGNED;
        auto timeout_time = NOW + std::chrono::seconds(seconds);
        auto check_exist_response = [&token]{
            return CoapClient::messages->exists(token);
        };
        CoapClient::client_condition.wait_until(lock, timeout_time, check_exist_response);

        if (check_exist_response())
            response = CoapClient::messages->get(token);
        else {
            response.status_code = GATEWAY_TIMEOUT;
            LOG_DBG("request timed out");
        }
    };

    std::thread t(task);
    t.join();

    CoapClient::messages->erase(token);
    return response;
}

message_t CoapClient::send_message_and_wait_response(request_t request, int timeout) {
    std::string token = this->send_message(request);
    return this->receive_message(token, timeout);
}
