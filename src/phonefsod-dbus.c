/*
 *  Copyright (C) 2009-2010
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

#include <string.h>
#include <gio/gio.h>
#include <shr-bindings.h>
#include "phonefsod-dbus.h"
#include "phonefsod-dbus-common.h"
#include "phonefsod-fso.h"
#include "phonefsod-globals.h"

static guint phonefsod_owner_id = 0;
static guint phoneuid_watcher_id = 0;
static PhonefsoUsage *usage;

/* phonefso - dbus method handlers */
static gboolean _set_offline_mode(PhonefsoUsage *object, GDBusMethodInvocation *invocation, gboolean state, gpointer user_data);
static gboolean _get_offline_mode(PhonefsoUsage *object, GDBusMethodInvocation *invocation, gpointer user_data);
static gboolean _set_default_brightness(PhonefsoUsage *object, GDBusMethodInvocation *invocation, int brightness, gpointer user_data);
static gboolean _get_default_brightness(PhonefsoUsage *object, GDBusMethodInvocation *invocation, gpointer user_data);
static gboolean _set_pdp_credentials(PhonefsoUsage *object, GDBusMethodInvocation *invocation, const char *apn, const char *user, const char *password, gpointer user_data);
static gboolean _set_pin(PhonefsoUsage *object, GDBusMethodInvocation *invocation, const char *pin, gboolean save, gpointer user_data);


/* private helper functions */
static void _write_pdp_credentials_to_config(void);
static void _write_default_brightness_to_config(void);
static void _write_offline_mode_to_config(void);

/* g_bus_own_name callbacks */
static void _on_bus_acquired (GDBusConnection *connection, const gchar *name, gpointer user_data);
static void _on_name_acquired (GDBusConnection *connection, const gchar *name, gpointer user_data);
static void _on_name_lost (GDBusConnection *connection, const gchar *name, gpointer user_data);

/* g_bus_watch_proxy callbacks */
static void _on_phoneuid_appeared(GDBusConnection *connection, const gchar *name, const gchar *name_owner, gpointer user_data);
static void _on_phoneuid_vanished(GDBusConnection *connection, const gchar *name, gpointer user_data);
static gint _show_sim_auth();

/* handle dbus errors */
static void _handle_dbus_error(GError *error, const gchar *msg);
static void _handle_phoneuid_proxy_error(GError *error, const gchar *iface);


int
phonefsod_dbus_setup()
{
	GError *error = NULL;

	system_bus = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
	if (error) {
		g_error("%d: %s", error->code, error->message);
		g_error_free(error);
		return 0;
	}

	phonefsod_owner_id = g_bus_own_name_on_connection
		(system_bus, PHONEFSOD_SERVICE,
		 G_BUS_NAME_OWNER_FLAGS_REPLACE | G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT,
		 _on_bus_acquired, _on_name_acquired, _on_name_lost, NULL);

	phoneuid_watcher_id = g_bus_watch_name_on_connection
		(system_bus, PHONEUID_SERVICE,
		 G_BUS_NAME_WATCHER_FLAGS_NONE,
		 _on_phoneuid_appeared, _on_phoneuid_vanished, NULL, NULL);

	phoneui.notification = phoneui_notification_proxy_new_sync
		(system_bus, G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
		PHONEUID_SERVICE, PHONEUID_NOTIFICATION_PATH, NULL, &error);
	_handle_phoneuid_proxy_error(error, PHONEUID_NOTIFICATION_PATH);

	phoneui.call_management = phoneui_call_management_proxy_new_sync
		(system_bus, G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
		PHONEUID_SERVICE, PHONEUID_CALL_MANAGEMENT_PATH, NULL, &error);
	_handle_phoneuid_proxy_error(error, PHONEUID_CALL_MANAGEMENT_PATH);

	phoneui.idle_screen = phoneui_idle_screen_proxy_new_sync
		(system_bus, G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
		PHONEUID_SERVICE, PHONEUID_IDLE_SCREEN_PATH, NULL, &error);
	_handle_phoneuid_proxy_error(error, PHONEUID_IDLE_SCREEN_PATH);

	phoneui.messages = phoneui_messages_proxy_new_sync
		(system_bus, G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
		PHONEUID_SERVICE, PHONEUID_MESSAGES_PATH, NULL, &error);
	_handle_phoneuid_proxy_error(error, PHONEUID_MESSAGES_PATH);

	/* connect and init FSO */
	if (!fso_init())
		return 0;

	return 1;
}

void
phonefsod_dbus_shutdown()
{
	g_bus_unown_name(phonefsod_owner_id);
	g_object_unref(system_bus);
}


/* handlers for g_bus_own_name */

static void
_on_bus_acquired (GDBusConnection *connection,
		   const gchar     *name,
		   gpointer         user_data)
{
	/* This is where we'd export some objects on the bus */
	GError *error = NULL;

	g_debug("Yo, on the bus :-) (%s)", name);

	usage = phonefso_usage_skeleton_new();
	g_signal_connect(usage, "handle-set-offline-mode", G_CALLBACK(_set_offline_mode), NULL);
	g_signal_connect(usage, "handle-get-offline-mode", G_CALLBACK(_get_offline_mode), NULL);
	g_signal_connect(usage, "handle-set-default-brightness", G_CALLBACK(_set_default_brightness), NULL);
	g_signal_connect(usage, "handle-get-default-brightness", G_CALLBACK(_get_default_brightness), NULL);
	g_signal_connect(usage, "handle-set-pdp-credentials", G_CALLBACK(_set_pdp_credentials), NULL);
	g_signal_connect(usage, "handle-set-pin", G_CALLBACK(_set_pin), NULL);


	g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(usage),
					     connection,
					     PHONEFSOD_USAGE_PATH,
					     &error);

	if (error) {
		g_critical("Failed to register %s: %s", PHONEFSOD_USAGE_PATH, error->message);
		g_error_free(error);
	}
}

static void
_on_name_acquired (GDBusConnection *connection,
		    const gchar     *name,
		    gpointer         user_data)
{
	g_debug ("Acquired the name %s on the system bus\n", name);
}

static void
_on_name_lost (GDBusConnection *connection,
		const gchar     *name,
		gpointer         user_data)
{
	g_debug ("Lost the name %s on the system bus\n", name);
}


/* handlers for g_bus_watch_name */

static void
_on_phoneuid_appeared(GDBusConnection *connection,
			 const gchar *name,
			 const gchar *name_owner,
			 gpointer user_data)
{
	g_debug("yeah, phoneuid is on the bus (%s)", name_owner);

	if (sim_auth_needed && phoneui.notification) {
		g_timeout_add_seconds(2, _show_sim_auth, NULL);
	}
}

static void
_on_phoneuid_vanished(GDBusConnection *connection,
			 const gchar *name,
			 gpointer user_data)
{
	g_message("!!! ouch, phoneuid is gone - telephony won't work anymore !!!");
}


static gint
_show_sim_auth()
{
	g_debug("showing PIN dialog");
	phoneui_notification_call_display_sim_auth
		(phoneui.notification, 0, NULL,
		 phoneui_show_sim_auth_cb, NULL);
}


/* method call handlers */
static gboolean
_set_offline_mode(PhonefsoUsage *object,
		GDBusMethodInvocation *invocation,
		gboolean state,
		gpointer user_data)
{
	if (offline_mode ^ state) {
		offline_mode = state;
		_write_offline_mode_to_config();
		fso_set_functionality();
	}

	phonefso_usage_complete_set_offline_mode(object, invocation);

	return TRUE;
}

static gboolean
_get_offline_mode(PhonefsoUsage *object,
		    GDBusMethodInvocation *invocation,
		    gpointer user_data)
{
	phonefso_usage_complete_get_offline_mode(object, invocation, offline_mode);

	return TRUE;
}

static gboolean
_set_default_brightness(PhonefsoUsage *object,
			   GDBusMethodInvocation *invocation,
			   int brightness,
			   gpointer user_data)
{
	default_brightness = brightness;
	_write_default_brightness_to_config();
	fso_dimit(100, DIM_SCREEN_ALWAYS);

	phonefso_usage_complete_set_default_brightness(object, invocation);
}

static gboolean
_get_default_brightness(PhonefsoUsage *object,
			   GDBusMethodInvocation *invocation,
			   gpointer user_data)
{
	phonefso_usage_complete_get_default_brightness(object, invocation, default_brightness);

	return TRUE;
}

static gboolean
_set_pdp_credentials(PhonefsoUsage *object,
			GDBusMethodInvocation *invocation,
			const char *apn,
			const char *user,
			const char *password,			gpointer user_data)
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

	phonefso_usage_complete_set_pdp_credentials(object, invocation);

	return TRUE;
}

static gboolean
_set_pin(PhonefsoUsage *object,
	  GDBusMethodInvocation *invocation,
	  const char *pin,
	  gboolean save,
	  gpointer user_data)
{
	if (sim_pin)
		free(sim_pin);
	sim_pin = strdup(pin);
	fso_set_functionality();
	if (save) {
		/* TODO: save to config... maybe including ident of SIM
		 *		to be able to save PINs for multiple SIMs */
	}

	phonefso_usage_complete_set_pin(object, invocation);

	return TRUE;
}


/* private helpers */

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


/* phoneuid - dbus callbacks */

void phoneui_show_incoming_cb(GObject *source, GAsyncResult *res, gpointer data)
{
	GError *error = NULL;
	phoneui_call_management_call_display_incoming_finish
			(PHONEUI_CALL_MANAGEMENT(source), res, &error);
	_handle_dbus_error(error, "failed showing incoming call");
}

void phoneui_hide_incoming_cb(GObject *source, GAsyncResult *res, gpointer data)
{
	GError *error = NULL;
	phoneui_call_management_call_hide_incoming_finish
			(PHONEUI_CALL_MANAGEMENT(source), res, &error);
	_handle_dbus_error(error, "failed hiding incoming call");
}

void phoneui_show_outgoing_cb(GObject *source, GAsyncResult *res, gpointer data)
{
	GError *error = NULL;
	phoneui_call_management_call_display_outgoing_finish
			(PHONEUI_CALL_MANAGEMENT(source), res, &error);
	_handle_dbus_error(error, "failed showing outgoing call");
}

void phoneui_hide_outgoing_cb(GObject *source, GAsyncResult *res, gpointer data)
{
	GError *error = NULL;
	phoneui_call_management_call_hide_outgoing_finish
			(PHONEUI_CALL_MANAGEMENT(source), res, &error);
	_handle_dbus_error(error, "failed hiding outgoing call");
}

void phoneui_display_message_cb(GObject *source, GAsyncResult *res, gpointer data)
{
	GError *error = NULL;
	phoneui_messages_call_display_message_finish
			(PHONEUI_MESSAGES(source), res, &error);
	_handle_dbus_error(error, "failed displaying message");
}

void phoneui_show_dialog_cb(GObject *source, GAsyncResult *res, gpointer data)
{
	GError *error = NULL;
	phoneui_notification_call_display_dialog_finish
			(PHONEUI_NOTIFICATION(source), res, &error);
	_handle_dbus_error(error, "failed displaying dialog");
}

void phoneui_show_sim_auth_cb(GObject *source, GAsyncResult *res, gpointer data)
{
	GError *error = NULL;
	phoneui_notification_call_display_sim_auth_finish
			(PHONEUI_NOTIFICATION(source), res, &error);
	/* if showing sim auth worked... and _only_ then */
	if (!error)
		sim_auth_needed = FALSE;
	_handle_dbus_error(error, "failed displaying sim auth");
}

void phoneui_hide_sim_auth_cb(GObject *source, GAsyncResult *res, gpointer data)
{
	GError *error = NULL;
	phoneui_notification_call_hide_sim_auth_finish
			(PHONEUI_NOTIFICATION(source), res, &error);
	_handle_dbus_error(error, "failed hiding sim auth");
}

void phoneui_show_ussd_cb(GObject *source, GAsyncResult *res, gpointer data)
{
	GError *error = NULL;
	phoneui_notification_call_display_ussd_finish
			(PHONEUI_NOTIFICATION(source), res, &error);
	_handle_dbus_error(error, "failed displaying ussd");
}

void phoneui_show_idle_cb(GObject *source, GAsyncResult *res, gpointer data)
{
	GError *error = NULL;
	phoneui_idle_screen_call_display_finish
			(PHONEUI_IDLE_SCREEN(source), res, &error);
	_handle_dbus_error(error, "failed displaying idle screen");
}

void phoneui_hide_idle_cb(GObject *source, GAsyncResult *res, gpointer data)
{
	GError *error = NULL;
	phoneui_idle_screen_call_hide_finish
			(PHONEUI_IDLE_SCREEN(source), res, &error);
	_handle_dbus_error(error, "failed hiding idle screen");
}

void phoneui_toggle_idle_cb(GObject *source, GAsyncResult *res, gpointer data)
{
	GError *error = NULL;
	phoneui_idle_screen_call_toggle_finish
			(PHONEUI_IDLE_SCREEN(source), res, &error);
	_handle_dbus_error(error, "failed toggling idle screen");
}

void phoneui_activate_screensaver_cb(GObject *source, GAsyncResult *res, gpointer data)
{
	GError *error = NULL;
	phoneui_idle_screen_call_activate_screensaver_finish
			(PHONEUI_IDLE_SCREEN(source), res, &error);
	_handle_dbus_error(error, "failed activating screensaver");
}

void phoneui_deactivate_screensaver_cb(GObject *source, GAsyncResult *res, gpointer data)
{
	GError *error = NULL;
	phoneui_idle_screen_call_deactivate_screensaver_finish
			(PHONEUI_IDLE_SCREEN(source), res, &error);
	_handle_dbus_error(error, "failed deactivating screensaver");
}


void _handle_dbus_error(GError* error, const gchar* msg)
{
	if (error) {
		g_critical("%s: (%d) %s", msg, error->code, error->message);
		g_error_free(error);
	}
}

void _handle_phoneuid_proxy_error(GError *error, const gchar *iface)
{
	if (error) {
		g_warning("getting proxy for %s failed: (%d) %s", iface, error->code, error->message);
		g_error_free(error);
		error = NULL;
	}
}
