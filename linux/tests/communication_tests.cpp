/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileCopyrightText: Huawei Inc.
 * SPDX-FileCopyrightText: Politecnico Di Milano
 */

#include <string>
#include <gtest/gtest.h>

#include "CoapClient.h"
#include "CoapServer.h"
#include "EddieEndpoint.h"

class EddieLamp : public EddieResource {
private:
    const char * const core_path[2] = { "test-lamp", nullptr };
    const char * const core_attributes[3] = { "rt=eddie.lamp", "ct=40", nullptr };

    int lamp_status = 0;

public:

    message_t render_get(request_t &request) override {
        message_t response;
        response.data = lamp_status == 0 ? "off" : "on";
        response.status_code = CONTENT;
        return response;
    }

    message_t render_post(request_t &request) override {
        message_t response;
        std::string request_payload_str = std::string((const char*)request.data, request.data_length);
        if (request_payload_str == "on") lamp_status = 1;
        else if (request_payload_str ==  "off") lamp_status = 0;
        else {
            response.status_code = BAD_REQUEST;
            return response;
        }
        response.status_code = CHANGED;
        return response;
    }

    message_t render_put(request_t &request) override {
        return render_post(request);
    }

    const char * const* get_path() override {
        return this->core_path;
    }

    const char * const* get_attributes() override {
        return this->core_attributes;
    }
};

TEST(Communication, Client_Requests) {
    CoapClient client;
    request_t request;
    request.dst_host = "::";
    request.dst_port = "5683";

    request.method = GET;
    request.path = "rd";
    request.query = "ep=sazz&base=pazz";
    client.send_message(request);

    request.method = POST;
    client.send_message(request);

    request.method = DELETE;
    request.query = nullptr;
    client.send_message(request);

    request.method = GET;
    request.path = ".well-known/core";
    request.query = nullptr;
    request.confirmable = false;
    client.send_message(request);
}

TEST(Communication, Client_Requests_And_Response) {
    CoapClient client;
    request_t request;
    request.dst_host = "::1";
    request.dst_port = "5683";
    request.path = ".well-known/core";

    request.query = "rt=core.rd";
    message_t response = client.send_message_and_wait_response(request);
    EXPECT_EQ(response.status_code, CONTENT);
    EXPECT_EQ(response.data, "</rd>;ct=40;rt=\"core.rd\"");

    request.query = "rt=core.rd-lookup-res";
    response = client.send_message_and_wait_response(request);
    EXPECT_EQ(response.status_code, CONTENT);
    EXPECT_EQ(response.data, "</rd-lookup/res>;ct=40;rt=\"core.rd-lookup-res\";obs");

    request.query = "rt=core.rd-lookup-ep";
    response = client.send_message_and_wait_response(request);
    EXPECT_EQ(response.status_code, CONTENT);
    EXPECT_EQ(response.data, "</rd-lookup/ep>;ct=40;rt=\"core.rd-lookup-ep\"");
}

TEST(Communication, Server_Init) {
    /* Resource dir disabled */
    CoapServer server = CoapServer("0.0.0.0", "5683");
}

TEST(Communication, Endpoint_Init) {
    EddieEndpoint endpoint("5684");
}

TEST(Communication, Add_Resource) {
    EddieEndpoint endpoint("5684");
    EddieLamp lamp_resource;
    EXPECT_EQ(endpoint.add_resource(&lamp_resource), 0);
}

TEST(Communication, Rd_Discovery) {
    EddieEndpoint endpoint("5684");
    EXPECT_EQ(endpoint.discover_rd(), 0);
}

TEST(Communication, Get_Resources_From_Rd) {
    EddieEndpoint endpoint("5684");

    EXPECT_EQ(endpoint.discover_rd(), 0);
    std::vector<Link> res = endpoint.get_resources_from_rd();
    EXPECT_EQ(res.size(), 1);
}

TEST(Communication, Publish_And_Get_Resources) {
    EddieEndpoint endpoint("5684", "::1");

    EXPECT_EQ(endpoint.discover_rd(), 0);

    EddieLamp lamp_resource;
    EXPECT_EQ(endpoint.add_resource(&lamp_resource), 0);
    EXPECT_EQ(endpoint.publish_resources(), 0);

    EXPECT_EQ(endpoint.start_server(), 0);

    std::vector<Link> res = endpoint.get_resources_from_rd();
    EXPECT_GT(res.size(), 0);

    Link first_link = res[0];
    std::string rt = first_link.attrs["rt"];
    EXPECT_EQ(rt, "eddie.lamp");
    EXPECT_EQ(first_link.host, "::1");
    EXPECT_EQ(first_link.port, "5684");
    EXPECT_EQ(first_link.path, "test-lamp");

    request_t request;
    request.dst_host = first_link.host.c_str();
    request.dst_port = first_link.port.c_str();
    request.path = first_link.path.c_str();
    request.method = POST;
    request.data = reinterpret_cast<const uint8_t *>("on");
    request.data_length = strlen("on");
    message_t response = endpoint.get_client()->send_message_and_wait_response(request);
    EXPECT_EQ(response.status_code, CHANGED);

    request.method = GET;
    request.data = nullptr;
    request.data_length = 0;
    response = endpoint.get_client()->send_message_and_wait_response(request);
    EXPECT_EQ(response.status_code, CONTENT);
    EXPECT_EQ(response.data, "on");

    EXPECT_EQ(endpoint.stop_server(), 0);
}
