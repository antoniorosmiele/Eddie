/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileCopyrightText: Huawei Inc.
 * SPDX-FileCopyrightText: Politecnico Di Milano
 */

#include <cstring>
#include <fstream>
#include <sstream>
#include <json/json.h>
#include <iostream>

#include "EddieTemperature.h"
#include "hardware_utils.h"

EddieTemperature::EddieTemperature(const std::string &path,
                                   const std::string &attributes,
                                   Json::Value root,
                                   bool on_board) {
    this->path = new char[path.length() + 1];
    strcpy(this->path, path.c_str());

    this->attributes = new char[attributes.length() + 1];
    strcpy(this->attributes, attributes.c_str());

    char *token = strtok(this->path, "/");
    while (token) {
        split_path.push_back(token);
        token = strtok(nullptr, "/");
    }
    split_path.push_back(nullptr);

    token = strtok(this->attributes, "&");
    while (token) {
        split_attributes.push_back(token);
        token = strtok(nullptr, "&");
    }
    split_attributes.push_back(nullptr);

    this->range.min = root["range"]["low"].asString();
    this->range.max = root["range"]["high"].asString();
    this->accuracy = root["accuracy"].asString();
    this->unit = root["unit"].asString();

    Json::CharReaderBuilder readerBuilder;
    readerBuilder["indentation"] = "";
    std::string errors;
    Json::Value tmp;
    std::ifstream t(DESCRIPTORS_DIR + root["descriptor"].asString());
    Json::parseFromStream(readerBuilder, t, &tmp, &errors);
    Json::StreamWriterBuilder writerBuilder;
    writerBuilder["indentation"] = "";
    this->json_descriptor = Json::writeString(writerBuilder, tmp);

    this->device_id = root["connection"]["device_id"].asString();
    reader = new OneWireReader(this->device_id, on_board);
    position = root["position"].asString();
}

EddieTemperature::~EddieTemperature() {
    delete[] path;
    delete[] attributes;
}

message_t EddieTemperature::render_get(request_t &request) {
    message_t response;

    if (request.query == nullptr) {
        response.status_code = BAD_REQUEST;
        return response;
    }

    auto query = parse_query(request.query);

    Json::Value answer;
    if (query["rt"] == "true") answer["rt"] = "eddie.r.temperature";
    if (query["temperature"] == "true") {
        long temperature;

        try {
            temperature = reader->read_value();
        } catch (std::runtime_error &error) {
            std::cout << "Error: " << error.what() << "\n";
            response.status_code = INTERNAL_SERVER_ERROR;
            return response;
        }

        answer["temperature"] = std::to_string(temperature);
    }
    if (query["unit"] == "true") answer["unit"] = this->unit;
    if (query["range"] == "true") {
        answer["range"]["low"] = this->range.min;
        answer["range"]["high"] = this->range.max;
    }
    if (query["accuracy"] == "true") answer["accuracy"] = this->attributes;
    if (query["position"] == "true") answer["position"] = this->position;
    if (query["json"] == "true") answer["json"] = this->json_descriptor;

    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    response.data = Json::writeString(builder, answer);
    response.status_code = CONTENT;
    return response;
}

const char *const *EddieTemperature::get_path() {
    return &split_path[0];
}

const char *const *EddieTemperature::get_attributes() {
    return &split_attributes[0];
}
