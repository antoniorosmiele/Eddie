/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileCopyrightText: Huawei Inc.
 * SPDX-FileCopyrightText: Politecnico Di Milano
 */

#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <cstdio>

#include "GPIOWriter.h"

class LinuxFile {
private:
    int m_Handle;

public:
    explicit LinuxFile(const char *pFile, int flags = O_RDWR) {
        m_Handle = open(pFile, flags);
    }

    ~LinuxFile() {
        if (m_Handle != -1)
            close(m_Handle);
    }

    size_t Write(const void *pBuffer, size_t size) const {
        return write(m_Handle, pBuffer, size);
    }

    size_t Read(void *pBuffer, size_t size) const {
        return read(m_Handle, pBuffer, size);
    }

    size_t Write(const char *pText) const {
        return Write(pText, strlen(pText));
    }

    size_t Write(int number) const {
        char szNum[32];
        snprintf(szNum, sizeof(szNum), "%d", number);
        return Write(szNum);
    }
};

class LinuxGPIOExporter {
protected:
    int m_Number;

public:
    explicit LinuxGPIOExporter(int number) : m_Number(number) {
        LinuxFile("/sys/class/gpio/export", O_WRONLY).Write(number);
    }

    ~LinuxGPIOExporter() {
        LinuxFile("/sys/class/gpio/unexport", O_WRONLY).Write(m_Number);
    }
};

class LinuxGPIO : public LinuxGPIOExporter {
public:
    explicit LinuxGPIO(int number) : LinuxGPIOExporter(number) {}

    void SetValue(bool value) {
        char szFN[128];
        snprintf(szFN, sizeof(szFN), "/sys/class/gpio/gpio%d/value", m_Number);
        LinuxFile(szFN).Write(value ? "1" : "0");
    }

    void SetDirection(bool isOutput) {
        char szFN[128];
        snprintf(szFN, sizeof(szFN),
                 "/sys/class/gpio/gpio%d/direction", m_Number);
        LinuxFile(szFN).Write(isOutput ? "out" : "in");
    }
};

GPIOWriter::GPIOWriter(int pin, bool on_board) {
    this->pin = pin;
    this->on_board = on_board;
}

void GPIOWriter::write(int value) const {
    if (on_board) {
        LinuxGPIO gpio(pin);
        gpio.SetDirection(true);
        gpio.SetValue(value);
    }
}
