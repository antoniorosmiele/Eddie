/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileCopyrightText: Huawei Inc.
 * SPDX-FileCopyrightText: Politecnico Di Milano
 */

#ifndef EDDIE_COMMON_H
#define EDDIE_COMMON_H

#define EDDIE_INTERFACE_NAME "org.eddie.TestInterface"
#define EDDIE_OBJECT "/org/eddie/TestObject"
#define EDDIE_DBUS_NAME "org.eddie.TestServer"

#define LOG_DBG(_str, ...) printf(_str "\n", ##__VA_ARGS__)
#define LOG_INFO(_str, ...) printf(_str "\n", ##__VA_ARGS__)
#define LOG_ERR(_str, ...) printf(_str "\n", ##__VA_ARGS__)

#endif //EDDIE_COMMON_H
