/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileCopyrightText: Huawei Inc.
 * SPDX-FileCopyrightText: Politecnico Di Milano
 */

#include <unistd.h>

#include <sstream>
#include <fstream>
#include "EddieBlinkingLamp.h"
#include <json/json.h>
#include "hardware_utils.h"

EddieBlinkingLamp::EddieBlinkingLamp(const std::string &path,
                                     const std::string &attributes,
                                     Json::Value root, bool on_board) {
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

    Json::CharReaderBuilder readerBuilder;
    readerBuilder["indentation"] = "";
    std::string errors;
    Json::Value tmp;
    std::ifstream t(DESCRIPTORS_DIR + root["descriptor"].asString());
    Json::parseFromStream(readerBuilder, t, &tmp, &errors);
    Json::StreamWriterBuilder writerBuilder;
    writerBuilder["indentation"] = "";
    this->json_descriptor = Json::writeString(writerBuilder, tmp);

    status = blinking = false;
    writer = new GPIOWriter(root["connection"]["data_pin"].asInt(), on_board);

    position = root["position"].asString();
    color = root["color"].asString();
}

EddieBlinkingLamp::~EddieBlinkingLamp() {
    delete[] path;
    delete[] attributes;
    delete writer;
    blinking = false;

    if (blinker.joinable()) blinker.join();
}

message_t EddieBlinkingLamp::render_get(request_t &request) {
    message_t response;

    if (request.query == nullptr) {
        response.status_code = BAD_REQUEST;
        return response;
    }

    auto query = parse_query(request.query);

    Json::Value answer;
    if (query["rt"] == "true") answer["rt"] = "eddie.r.blinking.lamp";
    if (query["color"] == "true") answer["color"] = this->color;
    if (query["status"] == "true") answer["status"] = (this->status ? "on" : "off");
    if (query["blinking"] == "true") answer["blinking"] = (this->blinking ? "yes" : "no");
    if (query["on_time"] == "true") answer["on_time"] = std::to_string(this->on_time);
    if (query["off_time"] == "true") answer["off_time"] = std::to_string(this->off_time);
    if (query["position"] == "true") answer["position"] = this->position;
    if (query["json"] == "true") answer["json"] = json_descriptor;

    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    response.data = Json::writeString(builder, answer);
    response.status_code = CONTENT;
    return response;
}

message_t EddieBlinkingLamp::render_post(request_t &request) {
    message_t response;
    int status_on = -1, start_blinking = -1;

    if (request.query == nullptr) {
        response.status_code = BAD_REQUEST;
        return response;
    }

    auto query = parse_query(request.query);

    auto set_status_it = query.find("status");
    auto set_blinking_it = query.find("blinking");
    auto on_time_it = query.find("on_time");
    auto off_time_it = query.find("off_time");

    if (set_status_it != query.end()) {
        if (set_status_it->second == "true") {
            status_on = 1;
        } else if (set_status_it->second == "false") {
            status_on = 0;
        } else {
            response.status_code = BAD_REQUEST;
            return response;
        }
    }
    if (set_blinking_it != query.end()) {
        if (set_blinking_it->second == "true") {
            start_blinking = 1;
        } else if (set_blinking_it->second == "false") {
            start_blinking = 0;
        } else {
            response.status_code = BAD_REQUEST;
            return response;
        }
    }

    if (on_time_it != query.end()) {
        long tmp = strtol(on_time_it->second.c_str(), nullptr, 0);
        if (tmp > 0) this->on_time = tmp;
        else {
            response.status_code = BAD_REQUEST;
            return response;
        }
    }
    if (off_time_it != query.end()) {
        long tmp = strtol(on_time_it->second.c_str(), nullptr, 0);
        if (tmp > 0) this->on_time = tmp;
        else {
            response.status_code = BAD_REQUEST;
            return response;
        }
    }

    if (status_on >= 0) {
        blinking = false;
        if (blinker.joinable()) blinker.join();
        status = status_on;
        writer->write(status);
    }

    if (start_blinking >= 0) {
        blinking = false;
        if (blinker.joinable()) blinker.join();

        if (start_blinking) {
            blinking = start_blinking;
            blinker = std::thread([this]() {
                while (blinking) {
                    if (blinking) {
                        status = true;
                        writer->write(status);
                        sleep(on_time); // consider nanosleep() for finer sleeping periods
                    }

                    if (blinking) {
                        status = false;
                        writer->write(status);
                        sleep(off_time);
                    }
                }
            });
        }
    }

    response.status_code = CHANGED;
    return response;
}

message_t EddieBlinkingLamp::render_put(request_t &request) {
    return render_post(request);
}

const char *const *EddieBlinkingLamp::get_path() {
    return &split_path[0];
}

const char *const *EddieBlinkingLamp::get_attributes() {
    return &split_attributes[0];
}
