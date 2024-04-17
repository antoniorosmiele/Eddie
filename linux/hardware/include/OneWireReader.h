/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileCopyrightText: Huawei Inc.
 */

#ifndef EDDIE_ONEWIREREADER_H
#define EDDIE_ONEWIREREADER_H

#include <string>

#define W1_BASE_PATH "/sys/devices/w1_bus_master1/"

class OneWireReader {
private:
    std::string device_id;
    bool on_board;

public:
    explicit OneWireReader(std::string device_id, bool on_board);
    bool is_on_board();
    long read_value();
};

#endif //EDDIE_ONEWIREREADER_H
