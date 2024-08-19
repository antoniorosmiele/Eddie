/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileCopyrightText: Huawei Inc.
 * SPDX-FileCopyrightText: Politecnico Di Milano
 */

#ifndef EDDIE_VIRTUALIZATION_SENDER_H
#define EDDIE_VIRTUALIZATION_SENDER_H

#include <glib.h>
#include <unordered_map>
#include <string>
#include <vector>
#include <json/json.h>

/**
 * Create a connection over dbus with the VirtualizationReceiver component.
 * @return An identifier (never 0) that can be used to close the connection
 */
guint connect();

/**
 * Close the connection with VirtualizationReceiver
 * @param watcher_id The identifier of the connection
 */
void disconnect(guint watcher_id);

/**
 * Create a JSON request message to send to the VirtualizationReceiver component
 * @param action one of "find", "get" and "actuate"
 * @param action_description a string detailing the action to be performed; valid only if "action=actuate"
 * @param h_constraints JSON array containing the list of hard constraints the engine will apply to the request
 * @param o_functions JSON array containing the list of objective functions the engine will apply to the request
 * @return The JSON request message
 */
Json::Value compose_message(const std::string& action,
                            const std::string& action_description,
                            Json::Value h_constraints,
                            Json::Value o_functions);

/**
 * Translate a JSON message into a Virtualization Layer request
 * and send the request to the VirtualizationReceiver using dbus
 * @param message The JSON message to send
 * @return The response as a JSON-formatted string
 */
std::string send_message(const Json::Value& message);

/**
 * Start the dbus polling loop
 */
void run();

/**
 * Start the process of selection translating 
 * a JSON message, with constraints and resource, 
 * into a Virtualization Layer request
 * and send the request to the VirtualizationReceiver using dbus
 */

std::string selection(Json::Value h_constraints,Json::Value o_functions,int size);

#endif //EDDIE_VIRTUALIZATION_SENDER_H
