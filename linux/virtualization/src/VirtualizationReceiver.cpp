/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileCopyrightText: Huawei Inc.
 * SPDX-FileCopyrightText: Politecnico Di Milano
 */

#include <vector>
#include <glib.h>
#include <gio/gio.h>
#include <string>

#include "common.h"
#include "VirtualizationReceiver.h"
#include "EddieEndpoint.h"
#include "engine.h"

static const gchar *introspection_XML =
        "<node>"
        "  <interface name='org.eddie.TestInterface'>"
        "    <method name='Connect'>"
        "      <arg type='s' name='response' direction='out'/>"
        "    </method>"
        "    <method name='PerformAction'>"
        "      <arg type='s' name='request' direction='in'/>"
        "      <arg type='s' name='response' direction='out'/>"
        "    </method>"
        "  </interface>"
        "</node>";

EddieEndpoint *VirtualizationReceiver::eddie_endpoint;
std::vector<EddieResource *> VirtualizationReceiver::resources;
GDBusNodeInfo *VirtualizationReceiver::introspection_data = g_dbus_node_info_new_for_xml(introspection_XML, nullptr);
const GDBusInterfaceVTable VirtualizationReceiver::interface_vtable = {handle_method_call};
Engine VirtualizationReceiver::engine = Engine();

void VirtualizationReceiver::handle_method_call(GDBusConnection *connection, const gchar *sender,
                                                const gchar *object_path, const gchar *interface_name,
                                                const gchar *method_name, GVariant *parameters,
                                                GDBusMethodInvocation *invocation, gpointer user_data) {
    if (g_strcmp0(method_name, "Connect") == 0) {
        LOG_DBG("Received Connect method call");
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(s)", "CONNECTED"));
        return;
    } else if (g_strcmp0(method_name, "PerformAction") == 0) {
        LOG_DBG("Received PerformAction method call");
        gchar *payload;

        g_variant_get(parameters, "(s)", &payload);

        //LOG_DBG("Executed g_variant_get");

        auto result = engine.perform(std::string(payload));

        g_dbus_method_invocation_return_value(invocation, g_variant_new("(s)", result.c_str()));
    }
}

void VirtualizationReceiver::on_bus_acquired(GDBusConnection *connection, const gchar *name, gpointer user_data) {
    guint registration_id;

    registration_id = g_dbus_connection_register_object(connection,
                                                        EDDIE_OBJECT,
                                                        introspection_data->interfaces[0],
                                                        &interface_vtable,
                                                        nullptr, nullptr, nullptr);

    g_assert(registration_id > 0);
    printf("Started virtualization layer");
}

void VirtualizationReceiver::on_name_acquired(GDBusConnection *connection, const gchar *name, gpointer user_data) {
    LOG_DBG("[Virtualization Layer]: dbus name acquired -> %s", name);
}

void VirtualizationReceiver::on_name_lost(GDBusConnection *connection, const gchar *name, gpointer user_data) {
    LOG_ERR("[Virtualization Layer]: dbus name lost <- %s", name);
    LOG_DBG("[Virtualization Receiver] Un-publishing resources");
    eddie_endpoint->unpublish_resources();
    exit(-1);
}

VirtualizationReceiver::VirtualizationReceiver(const std::string& ip, const std::string& port) {
    eddie_endpoint = new EddieEndpoint(port, ip);
    if (eddie_endpoint->discover_rd())
        eddie_endpoint->init_resource_dir_local();
    engine.set_endpoint(eddie_endpoint);

    owner_id = g_bus_own_name(G_BUS_TYPE_SESSION,
                              EDDIE_DBUS_NAME,
                              G_BUS_NAME_OWNER_FLAGS_NONE,
                              on_bus_acquired,
                              on_name_acquired,
                              on_name_lost,
                              nullptr, nullptr);
}

int VirtualizationReceiver::run(const std::vector<EddieResource *>& local_resources) {
    eddie_endpoint->start_server();

    // resource publication
    for (auto local_resource : local_resources) {
        VirtualizationReceiver::resources.push_back(local_resource);
        VirtualizationReceiver::eddie_endpoint->add_resource(local_resource);
    }
    eddie_endpoint->publish_resources();

    // dbus init
    GMainLoop *loop;

    loop = g_main_loop_new(nullptr, FALSE);

    g_main_loop_run(loop);

    g_bus_unown_name(owner_id);
    g_dbus_node_info_unref(introspection_data);

    return 0;
}

void VirtualizationReceiver::stop() const {
    LOG_DBG("[Virtualization Receiver] Un-publishing resources");
    eddie_endpoint->unpublish_resources();

    g_bus_unown_name(owner_id);
    g_dbus_node_info_unref(introspection_data);
}
