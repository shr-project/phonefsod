/*
 *  Copyright (C) 2008
 *      Authors (alphabetical) :
 *              Marc-Olivier Barre <marco@marcochapeau.org>
 *              Julien Cassignol <ainulindale@gmail.com>
 *              Andreas Engelbredt Dalsgaard <andreas.dalsgaard@gmail.com>
 *              Klaus 'mrmoku' Kurzmann <mok@fluxnetz.de>
 *              quickdev
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Public License as published by
 *  the Free Software Foundation; version 2 of the license.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser Public License for more details.
 */

#include <glib.h>
#include <glib/gthread.h>
#include <dbus/dbus-glib-bindings.h>
#include <frameworkd-glib/frameworkd-glib-dbus.h>
#include <frameworkd-glib/ousaged/frameworkd-glib-ousaged.h>
#include "phonefsod-dbus-common.h"
#include "phonefsod-dbus-usage.h"
#include "phonefsod-globals.h"
#include "phonefsod-usage-service-glue.h"


G_DEFINE_TYPE(PhonefsodUsageService, phonefsod_usage_service, G_TYPE_OBJECT)


int resources[OUSAGED_RESOURCE_COUNT];


static void
phonefsod_usage_service_class_init(PhonefsodUsageServiceClass * klass)
{
	GError *error = NULL;

	/* Init the DBus connection, per-klass */
	klass->connection = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
	if (klass->connection == NULL) {
		g_warning("Unable to connect to dbus: %s", error->message);
		g_error_free (error);
		return;
	}

	dbus_g_object_type_install_info (PHONEFSOD_TYPE_USAGE_SERVICE,
			&dbus_glib_phonefsod_usage_service_object_info);
}

static void
phonefsod_usage_service_init(PhonefsodUsageService * object)
{
	GError *error = NULL;
	DBusGProxy *driver_proxy;
	int request_ret;
	int f;

	for (f = 0; f < OUSAGED_RESOURCE_COUNT; f++)
		resources[f] = 0;

	PhonefsodUsageServiceClass *klass =
		PHONEFSOD_USAGE_SERVICE_GET_CLASS(object);

	/* Register DBUS path */
	dbus_g_connection_register_g_object(klass->connection,
			PHONEFSOD_USAGE_PATH,
			G_OBJECT (object));

	/* Register the service name, the constant here are defined in dbus-glib-bindings.h */
	driver_proxy = dbus_g_proxy_new_for_name (klass->connection,
			DBUS_SERVICE_DBUS,
			DBUS_PATH_DBUS,
			DBUS_INTERFACE_DBUS);

	if (!org_freedesktop_DBus_request_name (driver_proxy,
			PHONEFSOD_USAGE_NAME, 0, &request_ret, &error)) {
		g_warning("Unable to register service: %s", error->message);
		g_error_free (error);
	}
	g_object_unref(driver_proxy);
}


PhonefsodUsageService *
phonefsod_usage_service_new(void)
{
	return g_object_new(PHONEFSOD_TYPE_USAGE_SERVICE, NULL);
}


void
phonefsod_usage_get_resource_state_callback(GError * error, gboolean state,
					     gpointer userdata)
{
	DBusGMethodInvocation *context = (DBusGMethodInvocation *) userdata;
	if (error != NULL)
		dbus_g_method_return_error(context, error);
	else
		dbus_g_method_return(context, state);
}

void
phonefsod_usage_service_get_resource_state(PhonefsodUsageService * object,
					    const char *resource,
					    DBusGMethodInvocation * context)
{
	if (resource != NULL)
		ousaged_get_resource_state(resource,
					   phonefsod_usage_get_resource_state_callback,
					   context);
}

typedef struct {
	DBusGMethodInvocation *context;
	char *resource;
	int res;
} phonefsod_usage_request_resource_data_t;

void
phonefsod_usage_request_resource_callback(GError * error, gpointer userdata)
{
	phonefsod_usage_request_resource_data_t *data = userdata;

	if (error != NULL) {
		g_debug("error: %s", error->message);
	}
	else {
		g_debug("requested resource %s", data->resource,
			resources[data->res]);
		resources[data->res] = 1;
	}
	g_free(data->resource);
	dbus_g_method_return(data->context);
}

void
phonefsod_usage_service_request_resource(PhonefsodUsageService * object,
					  const char *resource,
					  DBusGMethodInvocation * context)
{
	if (resource != NULL) {
		int res = ousaged_resource_name_to_int(resource);
		resources[res]++;
		if (resources[res] > 1) {
			dbus_g_method_return(context);
			return;
		}
		phonefsod_usage_request_resource_data_t *data =
			g_malloc(sizeof
				 (phonefsod_usage_request_resource_data_t));
		data->context = context;
		data->resource = g_strdup(resource);
		data->res = res;
		ousaged_request_resource(resource,
					 phonefsod_usage_request_resource_callback,
					 data);
	}
}

void
phonefsod_usage_release_resource_callback(GError * error, gpointer userdata)
{
	phonefsod_usage_request_resource_data_t *data = userdata;
	if (error != NULL) {
		g_debug("error: %s", error->message);
	}
	else {
		g_debug("released resource %s", data->resource);
	}
	resources[data->res] = 0;
	g_free(data->resource);
	dbus_g_method_return(data->context);
}

void
phonefsod_usage_service_release_resource(PhonefsodUsageService * object,
					  const char *resource,
					  DBusGMethodInvocation * context)
{
	if (resource != NULL) {
		int res = ousaged_resource_name_to_int(resource);
		resources[res]--;
		if (resources[res] > 0) {
			dbus_g_method_return(context);
			return;
		}
		phonefsod_usage_request_resource_data_t *data =
			g_malloc(sizeof
				 (phonefsod_usage_request_resource_data_t));
		data->context = context;
		data->resource = g_strdup(resource);
		data->res = res;
		ousaged_release_resource(resource,
					 phonefsod_usage_release_resource_callback,
					 data);
	}
}
