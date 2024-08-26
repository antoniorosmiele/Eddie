/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileCopyrightText: Huawei Inc.
 * SPDX-FileCopyrightText: Politecnico Di Milano
 */

#ifndef EDDIE_COAPCLIENT_H
#define EDDIE_COAPCLIENT_H

#include "common.h"
#include "eddie.h"

#include <coap3/coap.h>
#include <mutex>
#include <condition_variable>
#include <thread>

#define COAP_IP4_MULTICAST "224.0.1.187"
#define COAP_IP6_MULTICAST_LOCAL_LINK "FF02::FD"
#define COAP_IP6_MULTICAST_SITE_LOCAL "FF05::FD"


class CoapClient {
private:
    bool quit = false;
    coap_context_t *context = nullptr;
    std::string my_interface;

    coap_session_t *open_session(const char *dst_host, const char *dst_port);

    static ThreadSafeMap<std::string, message_t> *messages;

    std::string last_token;

    std::thread client_thread;
    static std::mutex client_mutex;
    static std::condition_variable client_condition;

    void set_quit(bool value);

    [[nodiscard]] bool get_quit() const;

    static coap_response_t
    message_handler(coap_session_t *session, const coap_pdu_t *sent, const coap_pdu_t *received,
                    coap_mid_t id);

public:

    /**
     * CoapClient constructor that initializes communication context and handlers. The constructor
     * starts a thread that runs the coap I/O processing function.
     * @param context A pre-initialized context that is used for CoAP client communication. If context == nullptr,
     * a new context will be created.
     */
    explicit CoapClient(coap_context_t *context = nullptr);

    /**
     * CoapClient destructor stops client thread and releases resources.
     */
    ~CoapClient();

    /**
     * Send a coap request. This method is non-blocking and does not wait for the response.
     * @param request The request to send. This parameter contains information such as destination and data of the request.
     * @return Token of the request that will be used for matching the response.
     */
    std::string send_message(request_t request);

    /**
     * Receive a response that matches a previously sent request
     * @param token Token of the request previously sent.
     * @param timeout Maximum amount of time in seconds to wait for a response message that matches the given token.
     * @return The response message that matches the request token. If no response is received before timeout, the message
     * will contain a GATEWAY_TIMEOUT status code.
     */
    static message_t receive_message(std::string token, int timeout=5);

    /**
     * Send a coap request and immediately wait for a response
     * @param request The request to send. This parameter contains information such as destination and data of the request.
     * @param timeout Maximum amount of time in seconds to wait for a response message that matches the given token.
     * @return The response message that matches the request token. If no response is received before timeout, the message
     * will contain a GATEWAY_TIMEOUT status code.
     */
    message_t send_message_and_wait_response(request_t request, int timeout=5);

    void set_interface(std::string interface);
};

#endif //EDDIE_COAPCLIENT_H
