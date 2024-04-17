/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileCopyrightText: Huawei Inc.
 * SPDX-FileCopyrightText: Politecnico Di Milano
 */

#include <thread>
#include <json/json.h>
#include <iostream>
#include "VirtualizationSender.h"
#include "eddie.h"

volatile sig_atomic_t stop;

void inthand(int signum) {
    stop = 1;
    std::cout << "Closing because ctrl+c\n";
    exit(-1);
}

/*
 * Application logic:
 *   while(1):
 *      take temperature from sensors
 *      if temperature is over the trigger
 *          send alarm (silent alarm)
 *      reset status
 */
int main(int argc, char *argv[]) {
    signal(SIGINT, inthand);
    guint watcher_id = connect();

    std::thread main([]() {
        run();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    Json::CharReader *reader = Json::CharReaderBuilder().newCharReader();
    JSONCPP_STRING errs;
    Json::Value h_c;
    h_c = Json::arrayValue;
    h_c[0]["rt"] = "eddie.r.temperature";

    Json::Value o_f;
    auto message = compose_message("find", "", h_c, {});
    auto answer = send_message(message);

    h_c.clear();

    auto res_link = parse_link_format(answer);

    while (!stop) {
        auto answers = std::vector<Json::Value>();
        // collect all temperatures from every sensor
        for (const auto &link: res_link) {
            h_c[0]["dst_ip"] = link.host;
            h_c[1]["dst_port"] = link.port;
            h_c[2]["dst_res"] = link.path;
            h_c[3]["res"] = Json::arrayValue;
            h_c[3]["res"][0] = "temperature";
            h_c[3]["res"][1] = "unit";

            answer = send_message(compose_message("get", "", h_c, {}));
            Json::Value json;

            if (!reader->parse(answer.c_str(), answer.c_str() + answer.length(), &json, &errs)) {
                printf("Error while parsing answer: %s\n", answer.c_str());
            } else {
                std::cout << answer + "\n";
                answers.push_back(json);
            }
        }

        h_c.clear();

        // check for temperature
        for (auto a: answers) {
            // maybe add a conversion from °F and K to °C?
            double temperature = stod(a["temperature"].asString());

            // easier to cool the sensor than heat it
            if (temperature < 15.0) {
                // start alarm procedure
                // activate ALL possible devices with alarms installed
                h_c = Json::arrayValue;
                h_c[0]["type"] = "eddie.r.alarm";
                o_f = Json::arrayValue;
                o_f[0]["feature"] = "person";
                o_f[1]["feature"] = "silent";

                answer = send_message(compose_message("actuate", "alarm", h_c, o_f));
                std:: cout << answer << "\n";
                std::this_thread::sleep_for(std::chrono::milliseconds(10000));
                // stop alarm and reset status
                h_c[0]["type"] = "eddie.r.alarm";
                answer = send_message(compose_message("actuate", "reset", h_c, {}));
                std:: cout << answer << "\n";
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10000));
    }

    disconnect(watcher_id);
    main.join();

}
