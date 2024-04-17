/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileCopyrightText: Huawei Inc.
 * SPDX-FileCopyrightText: Politecnico Di Milano
 */

#include <thread>
#include <csignal>
#include <fstream>
#include <json/json.h>
#include "eddie.h"
#include "common.h"
#include "EddieEndpoint.h"
#include "argparse.hpp"
#include "EddieBlinkingLamp.h"
#include "EddieTemperature.h"
#include "VirtualizationReceiver.h"
#include "EddieVirtualAlarm.h"

volatile sig_atomic_t stop;

void inthand(int signum) {
    stop = 1;
}

int main(int argc, char *argv[]) {
    signal(SIGINT, inthand);
    argparse::ArgumentParser program("eddie-endpoint");

    program.add_argument("--ip", "-a")
            .help("Ip address")
            .default_value(std::string{"auto"});

    program.add_argument("--port", "-p")
            .help("Port number")
            .scan<'i', int>()
            .default_value(5683);

    program.add_argument("--on_board", "-b")
            .help("Whether the application is running on a GPIO board or not")
            .default_value(false)
            .implicit_value(true);

    program.add_argument("--light_person", "-lp")
            .help("Add a blinking lamp on person as a resource")
            .default_value(false)
            .implicit_value(true);

    program.add_argument("--light_remote", "-lr")
            .help("Add a remote blinking light as a resource")
            .default_value(false)
            .implicit_value(true);

    program.add_argument("--temp1", "-t1")
            .help("Add a temperature sensor as a resource with id 28-3c01f0954b85")
            .default_value(false)
            .implicit_value(true);

    program.add_argument("--temp2", "-t2")
            .help("Add a temperature sensor as a resource with id 28-3c01f09582ff")
            .default_value(false)
            .implicit_value(true);

    program.add_argument("--virtual", "-v")
            .help("Add a virtual alarm as a resource")
            .default_value(false)
            .implicit_value(true);

    try {
        program.parse_args(argc, argv);
    }
    catch (const std::runtime_error &err) {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        std::exit(1);
    }

    auto port_number = program.get<int>("port");
    auto ip = program.get<std::string>("ip");
    if (ip == "auto") ip = "";

    // RESOURCE SETUP
    std::string base_path = DESCRIPTORS_DIR;
    Json::CharReaderBuilder builder;
    Json::Value root;
    builder["collectComments"] = true;
    builder["indentation"] = "";
    JSONCPP_STRING errs;

    std::vector<EddieResource *> resources = {};

    if (program["temp1"] == true) {
        std::ifstream ifs_temp(base_path + "/json/config/temp.json");
        if (!Json::parseFromStream(builder, ifs_temp, &root, &errs)) {
            LOG_DBG("Error parsing json file: %s, %s", errs.c_str(), "temp.json");
            exit(-1);
        }

        auto *temperature = new EddieTemperature("temp1",
                                                 "rt=eddie.r.temperature&ct=40",
                                                 root,
                                                 program["--on_board"] == true);

        resources.push_back(temperature);
    }

    if (program["temp2"] == true) {
        std::ifstream ifs_temp(base_path + "/json/config/temp2.json");
        if (!Json::parseFromStream(builder, ifs_temp, &root, &errs)) {
            LOG_DBG("Error parsing json file: %s, %s", errs.c_str(), "temp2.json");
            exit(-1);
        }

        auto *temperature = new EddieTemperature("temp2",
                                                 "rt=eddie.r.temperature&ct=40",
                                                 root,
                                                 program["--on_board"] == true);

        resources.push_back(temperature);
    }

    if (program["light_remote"] == true) {
        std::ifstream ifs_lamp(base_path + "/json/config/led_remote.json");
        if (!Json::parseFromStream(builder, ifs_lamp, &root, &errs)) {
            LOG_DBG("Error parsing json file: %s, %s", errs.c_str(), "led_remote.json");
            exit(-1);
        }

        auto *lamp = new EddieBlinkingLamp("lamp_r",
                                           "rt=eddie.r.blinking.lamp&ct=40",
                                           root,
                                           program["--on_board"] == true);
        resources.push_back(lamp);
    }

    if (program["light_person"] == true) {
        std::ifstream ifs_lamp(base_path + "/json/config/led.json");
        if (!Json::parseFromStream(builder, ifs_lamp, &root, &errs)) {
            LOG_DBG("Error parsing json file: %s, %s", errs.c_str(), "led.json");
            exit(-1);
        }

        auto *lamp = new EddieBlinkingLamp("lamp_p",
                                           "rt=eddie.r.blinking.lamp&ct=40",
                                           root,
                                           program["--on_board"] == true);
        resources.push_back(lamp);
    }

    if (program["virtual"] == true) {
        std::ifstream ifs_alarm(base_path + "/json/config/alarm.json");
        if (!Json::parseFromStream(builder, ifs_alarm, &root, &errs)) {
            LOG_DBG("Error parsing json file: %s, %s", errs.c_str(), "alarm.json");
            exit(-1);
        }

        auto alarm = new EddieVirtualAlarm("alarm1",
                                           "rt=eddie.r.virtual.alarm&ct=40",
                                           root,
                                           program["on_board"] == true);
        resources.push_back(alarm);
    }

    // EDDIE LAYERS START
    VirtualizationReceiver r = VirtualizationReceiver(ip, std::to_string(port_number));
    auto f = [&resources, &r]() {
        r.run(resources);
    };
    std::thread runner(f);

    // STOPPER
    auto stopper_lambda= [&r]() {
        while (!stop) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        std::cout << "stopping because ctrl+c\n";
        r.stop();
        exit(0);
    };
    std::thread stopper(stopper_lambda);

    runner.join();
}
