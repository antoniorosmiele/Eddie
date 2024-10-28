/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileCopyrightText: Huawei Inc.
 * SPDX-FileCopyrightText: Politecnico Di Milano
 */

#ifndef EDDIE_ENGINE_H
#define EDDIE_ENGINE_H

#include <string>
#include <vector>
#include <json/json.h>
#include "eddie.h"
#include "EddieEndpoint.h"
#include "DFS.h"

class Engine {
private:
    EddieEndpoint *eddie_endpoint;

    std::unordered_multimap<std::string, std::pair<Json::Value, Link>> json_link_by_rt;
    std::unordered_map<std::string, Link> link_by_href;

    Json::Value dictionary;

    void apply_hard_constraints(const Json::Value& constraints);

    std::multimap<int, Link> apply_objective_functions(const Json::Value& functions);

public:
    /**
     * Engine constructor.
     */
    Engine();

    /**
     * Engine destructor
     */
    ~Engine();

    /**
     * Perform the requested action on a device/peripheral present in the network.
     * @param command string in JSON format detailing the action to perform.
     * @return JSON formatted string explaining the result of the action.
     */
    std::string perform(const std::string &command);

    /**
     * Set the link to the communication layer.
     * @param ep Communication layer endpoint.
     */
    void set_endpoint(EddieEndpoint *ep);

    /**
     * Update the data on the available resources in the network
     */
    void update_resources();

};

#endif //EDDIE_ENGINE_H
