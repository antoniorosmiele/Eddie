/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileCopyrightText: Huawei Inc.
 * SPDX-FileCopyrightText: Politecnico Di Milano
 */

#include <thread>
#include <unordered_map>

#include "VirtualizationSender.h"
#include "eddie.h"
#include "common.h"

int main() {
    guint watcher_id = connect();

    std::thread main([]() {
        run();
    });

    Json::Value h_c;
    h_c = Json::arrayValue;
    h_c[0]["rt"] = "eddie.r.virtual.alarm";

    auto message = compose_message("find", "", h_c, {});
    auto answer = send_message(message);

    auto res_link = parse_link_format(answer);

    LOG_DBG("RESOURCES FOUND WITH rt=eddie.r.virtual.alarm:");

    for (const auto& res: res_link) {
        LOG_DBG("%s:%s@%s", res.host.c_str(), res.port.c_str(), res.path.c_str());
    }

    disconnect(watcher_id);
    main.join();
}
