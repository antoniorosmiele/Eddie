/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileCopyrightText: Huawei Inc.
 * SPDX-FileCopyrightText: Politecnico Di Milano
 */

#ifndef EDDIE_EDDIETEMPERATURE_H
#define EDDIE_EDDIETEMPERATURE_H

#include <json/json.h>

#include "eddie.h"
#include "OneWireReader.h"

class EddieTemperature : public EddieResource {
private:
    char *path;
    char *attributes;
    std::vector<char *> split_path;
    std::vector<char *> split_attributes;

    struct {
        std::string min;
        std::string max;
    } range;

    std::string accuracy;
    std::string unit;

    std::string device_id;
    OneWireReader *reader;
    std::string json_descriptor;
    std::string position;

public:
    explicit EddieTemperature(const std::string &path,
                              const std::string &attributes,
                              Json::Value root,
                              bool on_board);

    ~EddieTemperature();

    message_t render_get(request_t &request) override;

    const char *const *get_path() override;

    const char *const *get_attributes() override;
};

#endif //EDDIE_EDDIETEMPERATURE_H
