/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileCopyrightText: Huawei Inc.
 * SPDX-FileCopyrightText: Politecnico Di Milano
 */

#include "ResourceDirectory.h"

int main() {
    coap_set_log_level(LOG_DEBUG);
    ResourceDirectory resource_directory(get_local_node_ip());
    resource_directory.run();
}