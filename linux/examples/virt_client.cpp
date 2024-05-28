/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileCopyrightText: Huawei Inc.
 * SPDX-FileCopyrightText: Politecnico Di Milano
 */

#include <thread>
#include <unordered_map>

#include "VirtualizationSender.h"
#include "../communication/include/common.h"
#include "eddie.h"
#include "common.h"

int main() {
    guint watcher_id = connect();

    std::thread main([]() {
        run();
    });

    Json::Value h_c;
    h_c = Json::arrayValue;
    h_c[0]["rt"] = "eddie.r.fake";

    auto message = compose_message("find", "", h_c, {});
    auto answer = send_message(message);

    auto res_link = parse_link_format(answer);

    LOG_DBG("RESOURCES FOUND WITH rt=eddie.r.virtual.alarm:");

    for (const auto& res: res_link) {
        LOG_DBG("%s:%s@%s", res.host.c_str(), res.port.c_str(), res.path.c_str());
    }

    Json::Value h_cs, o_f;

    h_cs = Json::arrayValue;
    o_f = Json::arrayValue;

    h_cs[0]["size"] = res_link.size();

    int i = 1;

    for (auto &link : res_link)
    {
        h_cs[i]["dst_ip"] = link.host;
        h_cs[i]["dst_port"] = link.port;
        h_cs[i]["dst_res"] = link.path;

        std::map<std::string, std::string> *attrs = &(link.attrs);

        for (auto &e : *attrs)
            h_cs[i][e.first] = e.second;

        i++;                
    }

    //Constraints

    o_f[0] = MGM_MAX;

    auto messageS = compose_message("selection", "", h_cs, o_f);

    auto answerS = send_message(messageS);

    LOG_DBG("%s", answerS);
    

    disconnect(watcher_id);
    main.join();
}
