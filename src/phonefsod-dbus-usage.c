/*
 *  Copyright (C) 2008-2010
 *      Authors (alphabetical) :
 *              Klaus 'mrmoku' Kurzmann <mok@fluxnetz.de>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Public License as published by
 *  the Free Software Foundation; version 2 of the license or any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser Public License for more details.
 */

#include <stdlib.h>
#include <glib.h>
#include <glib/gthread.h>
#include <dbus/dbus-glib-bindings.h>
#include <freesmartphone.h>
#include "phonefsod-dbus-common.h"
#include "phonefsod-dbus-usage.h"
#include "phonefsod-fso.h"
#include "phonefsod-globals.h"
#include "phonefsod-usage-service-glue.h"


G_DEFINE_TYPE(PhonefsodUsageService, phonefsod_usage_service, G_TYPE_OBJECT)


static void
_write_offline_mode_to_config(void)
{
	GError *error = NULL;
	GKeyFile *keyfile;
	GKeyFileFlags flags;
	gsize size;
	char *config_data;

	keyfile = g_key_file_new();
	flags = G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS;
	if (g_key_file_load_from_file
	    (keyfile, PHONEFSOD_CONFIG, flags, &error)) {
		g_key_file_set_boolean(keyfile, "gsm",
				"offline_mode", offline_mode);
		config_data = g_key_file_to_data(keyfile, &size, NULL);
		if (!config_data) {
			g_message("could not convert config data to write \
					offline mode to config");
		}
		else {
			if (!g_file_set_contents(PHONEFSOD_CONFIG, config_data,
					size, &error))
			{
				g_warning("failed writing offline mode \
					to config: %s", error->message);
				g_error_free(error);
			}
			g_free(config_data);
		}
	}

	if (keyfile)
		g_key_file_free(keyfile);
}

static void
_write_default_brightness_to_config(void)
{
	GError *error = NULL;
	GKeyFile *keyfile;
	GKeyFileFlags flags;
	gsize size;
	char *config_data;

	keyfile = g_key_file_new();
	flags = G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS;
	if (g_key_file_load_from_file
	    (keyfile, PHONEFSOD_CONFIG, flags, &error)) {
		g_key_file_set_integer(keyfile, "idle",
				"default_brightness", default_brightness);
		config_data = g_key_file_to_data(keyfile, &size, NULL);
		if (!config_data) {
			g_message("could not convert config data to write \
					default brightness to config");
		}
		else {
			if (!g_file_set_contents(PHONEFSOD_CONFIG, config_data,
					size, &error))
			{
				g_warning("failed writing default brightness \
					to config: %s", error->message);
				g_error_free(error);
			}
			g_free(config_data);
		}
	}

	if (keyfile)
		g_key_file_free(keyfile);
}

static void
_write_pdp_credentials_to_config(void)
{
	GError *error = NULL;
	GKeyFile *keyfile;
	GKeyFileFlags flags;
	gsize size;
	char *config_data;

	keyfile = g_key_file_new();
	flags = G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS;
	if (g_key_file_load_from_file
	    (keyfile, PHONEFSOD_CONFIG, flags, &error)) {
		g_key_file_set_string(keyfile, "gsm", "pdp_apn", pdp_apn);
		g_key_file_set_string(keyfile, "gsm", "pdp_user", pdp_user);
		g_key_file_set_string(keyfile, "gsm", "pdp_password", pdp_password);
		config_data = g_key_file_to_data(keyfile, &size, NULL);
		if (!config_data) {
			g_message("could not convert config data to write \
					default brightness to config");
		}
		else {
			if (!g_file_set_contents(PHONEFSOD_CONFIG, config_data,
					size, &error))
			{
				g_warning("failed writing pdp credentials \
					to config: %s", error->message);
				g_error_free(error);
			}
			g_free(config_data);
		}
	}

	if (keyfile)
		g_key_file_free(keyfile);
}

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
	int f;

	PhonefsodUsageServiceClass *klass =
		PHONEFSOD_USAGE_SERVICE_GET_CLASS(object);

	/* Register DBUS path */
	dbus_g_connection_register_g_object(klass->connection,
			PHONEFSOD_USAGE_PATH,
			G_OBJECT (object));
}


PhonefsodUsageService *
phonefsod_usage_service_new(void)
{
	return g_object_new(PHONEFSOD_TYPE_USAGE_SERVICE, NULL);
}

void
phonefsod_usage_service_set_offline_mode(PhonefsodUsageService *object,
		gboolean state, DBusGMethodInvocation *context)
{
	if (offline_mode ^ state) {
		offline_mode = state;
		_write_offline_mode_to_config();
		fso_set_functionality();
	}

	dbus_g_method_return(context);
}

void
phonefsod_usage_service_get_offline_mode(PhonefsodUsageService *object,
		DBusGMethodInvocation *context)
{
	dbus_g_method_return(context, offline_mode);
}

void
phonefsod_usage_service_set_default_brightness(PhonefsodUsageService *object,
					       int brightness,
					       DBusGMethodInvocation *context)
{
	default_brightness = brightness;
	_write_default_brightness_to_config();
	fso_dimit(100, DIM_SCREEN_ALWAYS);
	dbus_g_method_return(context);
}

void
phonefsod_usage_service_get_default_brightness(PhonefsodUsageService *object,
					       DBusGMethodInvocation *context)
{
	dbus_g_method_return(context, default_brightness);
}

void
phonefsod_usage_service_set_pdp_credentials(PhonefsodUsageService* object,
					    const char *apn,
					    const char *user,
					    const char *password,
					    DBusGMethodInvocation* context)
{
	if (pdp_apn) {
		free(pdp_apn);
	}
	pdp_apn = strdup(apn);
	if (pdp_user) {
		free(pdp_user);
	}
	pdp_user = strdup(user);
	if (pdp_password) {
		free(pdp_password);
	}
	pdp_password = strdup(password);
	_write_pdp_credentials_to_config();
	fso_pdp_set_credentials();
	dbus_g_method_return(context);
}
