/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileCopyrightText: Huawei Inc.
 * SPDX-FileCopyrightText: Politecnico Di Milano
 */

#ifndef EDDIE_EDDIEDoor_H
#define EDDIE_EDDIEDoor_H

#include "eddie.h"
#include <json/json.h>
#include <thread>

class EddieDoor : public EddieResource {
private:
    char *path;
    char *attributes;
    std::vector<char *> split_path;
    std::vector<char *> split_attributes;

    bool status;
    std::string json_descriptor;
    std::string place;
    std::string name;

    std::thread alarm_thread;

public:
    explicit EddieDoor(const std::string &path,
                               const std::string &attributes,
                               Json::Value root,
                               bool on_board);

    ~EddieDoor();

    message_t render_get(request_t &request) override;

    message_t render_post(request_t &request) override;

    message_t render_put(request_t &request) override;

    const char *const *get_path() override;

    const char *const *get_attributes() override;
};

#endif //EDDIE_EDDIEVIRTUALALARM_H
