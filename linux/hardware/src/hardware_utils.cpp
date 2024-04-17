/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileCopyrightText: Huawei Inc.
 * SPDX-FileCopyrightText: Politecnico Di Milano
 */

#include "common.h"
#include "hardware_utils.h"

std::map<std::string, std::string> parse_query(const std::string& query) {
    auto result = std::map<std::string, std::string>();

    auto tokens = split(query, '&');
    for (const auto &token: tokens) {
        auto query_element = split(token, '=');
        result[query_element[0]] = query_element[1];
    }

    return result;
}
