/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileCopyrightText: Huawei Inc.
 * SPDX-FileCopyrightText: Politecnico Di Milano
 */

#ifndef EDDIE_EDDIEBLINKINGLAMP_H
#define EDDIE_EDDIEBLINKINGLAMP_H

#include <thread>
#include <cstring>
#include <json/json.h>
#include "eddie.h"
#include "GPIOWriter.h"

class EddieBlinkingLamp : public EddieResource {
private:
    char *path;
    char *attributes;
    std::vector<char *> split_path;
    std::vector<char *> split_attributes;

    bool status, blinking;
    std::thread blinker;

    long on_time = 1, off_time = 1;
    std::string color;
    std::string json_descriptor;
    GPIOWriter *writer;

    std::string position;

public:
    explicit EddieBlinkingLamp(const std::string &path,
                               const std::string &attributes,
                               Json::Value root,
                               bool on_board);

    ~EddieBlinkingLamp();

    message_t render_get(request_t &request) override;

    message_t render_post(request_t &request) override;

    message_t render_put(request_t &request) override;

    const char *const *get_path() override;

    const char *const *get_attributes() override;
};


#endif //EDDIE_EDDIEBLINKINGLAMP_H
