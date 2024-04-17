/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileCopyrightText: Huawei Inc.
 * SPDX-FileCopyrightText: Politecnico Di Milano
 */
#include <gtest/gtest.h>
#include <thread>
#include <unordered_map>
#include <json/json.h>

#include "VirtualizationSender.h"
#include "eddie.h"

TEST(Communication, Virtualization_Sender) {
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
    EXPECT_EQ(res_link.size(), 1);

    disconnect(watcher_id);

    main.join();
}