/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileCopyrightText: Huawei Inc.
 */

#include <utility>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

#include "OneWireReader.h"

OneWireReader::OneWireReader(std::string device_id, bool on_board) {
    this->device_id = std::move(device_id);
    this->on_board = on_board;
}

bool OneWireReader::is_on_board() {
    return this->on_board;
}

long OneWireReader::read_value() {
    /*
     * NOTE: this implementation is very specific to the DS-18B20 temperature sensor
     * consider a generalization to allow different sensors.
     */

    /*
     * The value of 9.0 is used for testing purposes.
     * Change it if some test requires it.
     */
    if (!this->on_board) return 9.0;

    std::string path = W1_BASE_PATH + this->device_id + "/w1_slave";
    int fd = open(path.c_str(), O_RDONLY);

    if (fd == -1) {
        throw std::runtime_error("File not found");
    }

    char buf[256];
    ssize_t data = read(fd, buf, 256);

    if (data <= 0) {
        throw std::runtime_error("Data not found");
    }

    std::string data_s = buf;
    if (data_s.find("YES") == std::string::npos) {
        throw std::runtime_error("Invalid data from sensor");
    }

    char temp_data[6];
    strncpy(temp_data, strstr(buf, "t=") + 2, 5);
    close(fd);
    return strtol(temp_data, nullptr, 10) / 1000;
}
