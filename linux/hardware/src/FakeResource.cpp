#include <cstring>
#include <unistd.h>
#include <fstream>
#include "FakeResource.h"
#include "hardware_utils.h"

FakeResource::FakeResource(const std::string &path,
                                     const std::string &attributes,
                                     Json::Value root,
                                     bool on_board,int key) {

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
    root["id"] =  std::to_string(key);
    std::ifstream t(DESCRIPTORS_DIR + root["descriptor"].asString());
    Json::parseFromStream(readerBuilder, t, &tmp, &errors);
    Json::StreamWriterBuilder writerBuilder;
    writerBuilder["indentation"] = "";
    this->json_descriptor = Json::writeString(writerBuilder, tmp);
}

FakeResource::~FakeResource() {
    delete[] path;
    delete[] attributes;
}

message_t FakeResource::render_get(request_t &request) 
{

}

message_t FakeResource::render_post(request_t &request) 
{

}

message_t FakeResource::render_put(request_t &request) 
{

}

const char *const *FakeResource::get_path() {
    return &split_path[0];
}

const char *const *FakeResource::get_attributes() {
    return &split_attributes[0];
}