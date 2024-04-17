/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileCopyrightText: Huawei Inc.
 * SPDX-FileCopyrightText: Politecnico Di Milano
 */

#include <thread>
#include <gio/gio.h>

#include "common.h"
#include "VirtualizationSender.h"

#define MAX_RETRIES 5

GDBusConnection *global_connection;
const gchar *global_name_owner;
GMainLoop *global_loop;

static int get_server(GDBusConnection *connection, const gchar *name_owner, GError **error) {
    GDBusMessage *method_call_message;
    GDBusMessage *method_reply_message;

    int fd = -1;
    const gchar *response;

    method_call_message = g_dbus_message_new_method_call(name_owner,
                                                         EDDIE_OBJECT,
                                                         EDDIE_INTERFACE_NAME,
                                                         "Connect");

    g_dbus_message_set_body(method_call_message, g_variant_new("()"));
    method_reply_message = g_dbus_connection_send_message_with_reply_sync(connection,
                                                                          method_call_message,
                                                                          G_DBUS_SEND_MESSAGE_FLAGS_NONE,
                                                                          -1,
                                                                          nullptr, /* out_serial */
                                                                          nullptr, /* cancellable */
                                                                          error);

    if (method_reply_message == nullptr) {
        g_object_unref(method_call_message);
        g_object_unref(method_reply_message);
        return fd;
    }

    if (g_dbus_message_get_message_type(method_reply_message) == G_DBUS_MESSAGE_TYPE_ERROR) {
        g_dbus_message_to_gerror(method_reply_message, error);
        g_printerr("[VirtualizationSender]: Received error on dbus");

        g_object_unref(method_call_message);
        g_object_unref(method_reply_message);
        return fd;
    }

    fd = 1;
    response = g_dbus_message_get_arg0(method_reply_message);
    return fd;
}

static void on_name_appeared(GDBusConnection *connection, const gchar *name, const gchar *name_owner,
                             gpointer user_data) {
    gint fd;
    GError *error;
    error = nullptr;

    fd = get_server(connection, name_owner, &error);

    if (fd == -1) {
        g_printerr("Error invoking Tuple: %s\n", error->message);
        g_error_free(error);
        exit(-1);
    } else {
        global_connection = connection;
        global_name_owner = name_owner;
    }
}

static void on_name_vanished(GDBusConnection *connection, const gchar *name, gpointer user_data) {
    g_printerr("Failed to get name owner for %s\n", name);
    exit(-1);
}

guint connect() {
    guint watcher_id;

    watcher_id = g_bus_watch_name(G_BUS_TYPE_SESSION,
                                  EDDIE_DBUS_NAME,
                                  G_BUS_NAME_WATCHER_FLAGS_NONE,
                                  on_name_appeared,
                                  on_name_vanished,
                                  nullptr, nullptr);

    return watcher_id;
}

void disconnect(guint watcher_id) {
    g_bus_unwatch_name(watcher_id);
    g_main_loop_quit(global_loop);
}

Json::Value compose_message(const std::string& action,
                            const std::string& action_description,
                            Json::Value h_constraints,
                            Json::Value o_functions) {
    Json::Value message;

    if (action.empty()) {
        return message;
    }
    message["action"] = action;

    if (!action_description.empty())
        message["action_description"] = action_description;

    if (!h_constraints.empty()) {
        message["h_constraints"] = std::move(h_constraints);
    }

    if (!o_functions.empty()) {
        message["o_function"] = std::move(o_functions);
    }

    return message;
}

std::string send_message(const Json::Value& message) {
    int count = 0;

    while (global_connection == nullptr && count < MAX_RETRIES) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        count++;
    }

    if (count >= MAX_RETRIES) {
        LOG_ERR("[VirtualizationSender]: no dbus found, impossible to send the message\n");
        return "error";
    }

    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    const std::string out = Json::writeString(builder, message);

    GVariant *value = g_variant_new("(s)", out.c_str());

    GDBusMessage *method_call_message;
    GDBusMessage *method_reply_message;
    GError *error;
    error = nullptr;

    method_call_message = g_dbus_message_new_method_call(global_name_owner,
                                                         EDDIE_OBJECT,
                                                         EDDIE_INTERFACE_NAME,
                                                         "PerformAction");

    g_dbus_message_set_body(method_call_message, value);

    method_reply_message = g_dbus_connection_send_message_with_reply_sync(global_connection,
                                                                          method_call_message,
                                                                          G_DBUS_SEND_MESSAGE_FLAGS_NONE,
                                                                          -1,
                                                                          nullptr,
                                                                          nullptr,
                                                                          &error);

    if (method_reply_message == nullptr) {
        g_object_unref(method_call_message);
        g_object_unref(method_reply_message);
        return "error";
    }

    if (g_dbus_message_get_message_type(method_reply_message) == G_DBUS_MESSAGE_TYPE_ERROR) {
        g_dbus_message_to_gerror(method_reply_message, &error);
        g_printerr("%s\n", error->message);
        g_object_unref(method_call_message);
        g_object_unref(method_reply_message);
        return "error";
    }

    const gchar *response = g_dbus_message_get_arg0(method_reply_message);
    std::string response_str = std::string(response);

    g_object_unref(method_call_message);
    g_object_unref(method_reply_message);

    return response_str;
}

void run() {
    global_loop = g_main_loop_new(nullptr, FALSE);
    g_main_loop_run(global_loop);
}
