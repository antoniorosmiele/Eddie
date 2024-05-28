/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileCopyrightText: Huawei Inc.
 * SPDX-FileCopyrightText: Politecnico Di Milano
 */

#include <vector>
#include <string>
#include <json/json.h>
#include <fstream>
#include "VirtualizationReceiver.h"
#include "argparse.hpp"
#include "EddieVirtualAlarm.h"

using namespace std;

int main(int argc, char *argv[]) {
    argparse::ArgumentParser program("eddie-virt-server");

    program.add_argument("--ip", "-a")
            .help("Ip address")
            .default_value(std::string{"auto"});

    program.add_argument("--port", "-p")
            .help("Port number")
            .scan<'i', int>()
            .default_value(5683);

    program.add_argument("--exampleres", "-e")
            .help("Publish example lamp resources")
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

    VirtualizationReceiver receiver = VirtualizationReceiver(ip, std::to_string(port_number));

    string base_path = DESCRIPTORS_DIR;
    ifstream ifs_alarm(base_path + "/json/config/alarm.json");
    Json::CharReaderBuilder builder;
    Json::Value root;
    builder["collectComments"] = true;
    builder["indentation"] = "";
    JSONCPP_STRING errs;
    if (!Json::parseFromStream(builder, ifs_alarm, &root, &errs)) {
        LOG_DBG("Error parsing json file: %s, %s", errs.c_str(), string(base_path + "alarm.json").c_str());
        exit(-1);
    }

    vector<EddieResource *> resources = {};
    if (program["--exampleres"] == true) {
        auto res = new EddieVirtualAlarm("alarm",
                                         "rt=eddie.r.virtual.alarm&ct=40",
                                         root,
                                         false);
        resources.push_back(res);
    }

    receiver.run(resources);
}
