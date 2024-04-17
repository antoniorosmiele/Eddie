/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileCopyrightText: Huawei Inc.
 * SPDX-FileCopyrightText: Politecnico Di Milano
 */

#ifndef EDDIE_GPIOWRITER_H
#define EDDIE_GPIOWRITER_H

class GPIOWriter {
private:
    int pin;
    bool on_board;
public:
    explicit GPIOWriter(int pin, bool on_board);
    void write(int value) const;
};

#endif //EDDIE_GPIOWRITER_H
