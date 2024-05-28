/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileCopyrightText: Huawei Inc.
 * SPDX-FileCopyrightText: Politecnico Di Milano
 */

#ifndef FAKE_RESOURCE_H
#define FAKE_RESOURCE_H

#include <json/json.h>

#include "eddie.h"
#include "OneWireReader.h"

class FakeResource : public EddieResource {
private:
    char *path;
    char *attributes;
    std::vector<char *> split_path;
    std::vector<char *> split_attributes;

    std::string json_descriptor;


public:
    explicit FakeResource(const std::string &path,
                               const std::string &attributes,
                               Json::Value root,
                               bool on_board,int key);

    ~FakeResource();

    message_t render_get(request_t &request) override;

    message_t render_post(request_t &request) override;

    message_t render_put(request_t &request) override;

    const char *const *get_path() override;

    const char *const *get_attributes() override;
};

#endif //EDDIE_EDDIETEMPERATURE_H