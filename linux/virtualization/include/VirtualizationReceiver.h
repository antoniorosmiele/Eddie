/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileCopyrightText: Huawei Inc.
 * SPDX-FileCopyrightText: Politecnico Di Milano
 */

#ifndef EDDIE_VIRTUALIZATION_RECEIVER_H
#define EDDIE_VIRTUALIZATION_RECEIVER_H

#include <glib.h>
#include <gio/gio.h>

#include "EddieEndpoint.h"
#include "engine.h"

class VirtualizationReceiver {
private:
    guint owner_id;
    static EddieEndpoint *eddie_endpoint;
    static std::vector<EddieResource*> resources;
    static GDBusNodeInfo *introspection_data;
    static const GDBusInterfaceVTable interface_vtable;
    static Engine engine;

    static void
    handle_method_call (GDBusConnection         *connection,
                        const gchar             *sender,
                        const gchar             *object_path,
                        const gchar             *interface_name,
                        const gchar             *method_name,
                        GVariant                *parameters,
                        GDBusMethodInvocation   *invocation,
                        gpointer                user_data);

    static void
    on_bus_acquired(GDBusConnection *connection, const gchar *name, gpointer user_data);

    static void
    on_name_acquired(GDBusConnection *connection, const gchar *name, gpointer user_data);

    static void
    on_name_lost(GDBusConnection *connection, const gchar *name, gpointer user_data);

public:
    /**
     * Virtualization constructor
     * Initialize the Communication layer component as well as acquiring ownership of the EDDIE dbus name
     * @param ip The Ip address used to initialize the communication layer components.
     * @param port The port number used to initialize the communication layer components.
     */
    explicit VirtualizationReceiver(const std::string& ip = "", const std::string& port = "5683");

    /**
     * Start the dbus polling loop and start the Communication layer.
     * @param local_resources list of EddieResources to add to the local node.
     * @return 0 on success, negative integer on failure.
     */
    int run(const std::vector<EddieResource*>& local_resources);

    /**
     * Remove the registration to the Resource Directory of this node
     * and release the EDDIE dbus name
     */
    void stop() const;
};

#endif //EDDIE_VIRTUALIZATION_RECEIVER_H
