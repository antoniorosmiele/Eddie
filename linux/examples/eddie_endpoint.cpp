/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileCopyrightText: Huawei Inc.
 */

#include "EddieEndpoint.h"
#include "eddie.h"
#include "argparse.hpp"

#include <csignal>
#include <chrono>
#include <thread>

volatile sig_atomic_t stop;

void inthand(int signum) {
    stop = 1;
}

class EddieLamp : public EddieResource {
private:
	const char * const core_path[2] = { "linux-lamp", nullptr };
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

class EddieTemperature : public EddieResource {
private:
	const char * const core_path[2] = { "linux-temp", nullptr };
	const char * const core_attributes[3] = { "rt=eddie.temp", "ct=40", nullptr };

public:

	message_t render_get(request_t &request) override {
		message_t response;
        response.data = "very hot";
		response.status_code = CONTENT;
		return response;
	}

	const char * const* get_path() override {
		return this->core_path;
	}

	const char * const* get_attributes() override {
		return this->core_attributes;
	}
};

int main(int argc, char *argv[])
{
    signal(SIGINT, inthand);
    argparse::ArgumentParser program("eddie-endpoint");

    program.add_argument("--ip", "-a")
            .help("Ip address")
            .default_value(std::string{"auto"});

    program.add_argument("--port", "-p")
            .help("Port number")
            .scan<'i', int>()
            .default_value(5683);

    program.add_argument("--exampleres", "-e")
            .help("Publish example lamp and temperature resources")
            .default_value(false)
            .implicit_value(true);

    try {
        program.parse_args(argc, argv);
    }
    catch (const std::runtime_error& err) {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        std::exit(1);
    }

    auto port_number = program.get<int>("port");
    auto ip = program.get<std::string>("ip");
    if (ip == "auto") ip = "";

    coap_set_log_level(LOG_DEBUG);

	EddieEndpoint node = EddieEndpoint(std::to_string(port_number), ip);
    if (node.discover_rd()) node.init_resource_dir_local();
    node.start_server();

    if (program["--exampleres"] == true) {
        EddieLamp lamp_resource;
        EddieTemperature temperature_resource;
        node.add_resource(&lamp_resource);
        node.add_resource(&temperature_resource);
        node.publish_resources();
    }

    std::vector<Link> discovered_resources = node.get_resources_from_rd();
    for (const Link& link: discovered_resources) {
        LOG_DBG("Discovered resource: host=%s, port=%s, path=%s", link.host.c_str(), link.port.c_str(), link.path.c_str());
    }

    while (!stop) std::this_thread::sleep_for(std::chrono::seconds(1));
	
	return 0;
}
