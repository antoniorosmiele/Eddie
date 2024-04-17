/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileCopyrightText: Huawei Inc.
 * SPDX-FileCopyrightText: Politecnico Di Milano
 */

#include <cstring>
#include <unistd.h>
#include <fstream>
#include "EddieVirtualAlarm.h"
#include "hardware_utils.h"

EddieVirtualAlarm::EddieVirtualAlarm(const std::string &path,
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

    status = false;
    Json::CharReaderBuilder readerBuilder;
    readerBuilder["indentation"] = "";
    std::string errors;
    Json::Value tmp;
    std::ifstream t(DESCRIPTORS_DIR + root["descriptor"].asString());
    Json::parseFromStream(readerBuilder, t, &tmp, &errors);
    Json::StreamWriterBuilder writerBuilder;
    writerBuilder["indentation"] = "";
    this->json_descriptor = Json::writeString(writerBuilder, tmp);
    this->position = root["position"].asString();
}

EddieVirtualAlarm::~EddieVirtualAlarm() {
    delete[] path;
    delete[] attributes;
}

message_t EddieVirtualAlarm::render_get(request_t &request) {
    message_t response;

    if (request.query == nullptr) {
        response.status_code = BAD_REQUEST;
        return response;
    }

    auto query = parse_query(request.query);

    Json::Value answer;
    if (query["rt"] == "true") answer["rt"] = "eddie.r.virtual.alarm";
    if (query["status"] == "true") answer["status"] = (this->status ? "on" : "off");
    if (query["position"] == "true") answer["position"] = this->position;
    if (query["json"] == "true") answer["json"] = this->json_descriptor;

    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    response.data = Json::writeString(builder, answer);
    response.status_code = CONTENT;
    return response;
}

message_t EddieVirtualAlarm::render_post(request_t &request) {
    message_t response;

    if (request.query == nullptr) {
        response.status_code = BAD_REQUEST;
        return response;
    }

    int status_on = -1;

    auto query = parse_query(request.query);
    auto set_status_it = query.find("status");

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

    if (status_on >= 0) {
        status = false;
        if (alarm_thread.joinable()) alarm_thread.join();

        if (status_on) {
            status = status_on;
            alarm_thread = std::thread([this]() {
                while (status) {
                    if (status) {
                        printf("....###....##..........###....########..##.....##\n"
                               "...##.##...##.........##.##...##.....##.###...###\n"
                               "..##...##..##........##...##..##.....##.####.####\n"
                               ".##.....##.##.......##.....##.########..##.###.##\n"
                               ".#########.##.......#########.##...##...##.....##\n"
                               ".##.....##.##.......##.....##.##....##..##.....##\n"
                               ".##.....##.########.##.....##.##.....##.##.....##\n");
                        sleep(2);
                    }
                }
            });
        }
    }

    response.status_code = CHANGED;
    return response;
}

message_t EddieVirtualAlarm::render_put(request_t &request) {
    return render_post(request);
}

const char *const *EddieVirtualAlarm::get_path() {
    return &split_path[0];
}

const char *const *EddieVirtualAlarm::get_attributes() {
    return &split_attributes[0];
}
