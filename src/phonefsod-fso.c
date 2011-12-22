/*
 *  Copyright (C) 2009-2011
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
#include <string.h>
#include <time.h>
#include <glib.h>
#include <glib/gthread.h>
#include <gio/gio.h>
#include <freesmartphone.h>
#include <shr-bindings.h>
#include "phonefsod-dbus.h"
#include "phonefsod-fso.h"
#include "phonefsod-globals.h"
#include "phonefsod-dbus-common.h"

#define MIN_SIM_SLOTS_FREE 1

struct _fso {
	FreeSmartphoneUsage *usage;
	FreeSmartphoneGSMSIM * gsm_sim;
	FreeSmartphoneGSMCall *gsm_call;
	FreeSmartphoneGSMDevice *gsm_device;
	FreeSmartphoneGSMNetwork *gsm_network;
	FreeSmartphoneGSMPDP *gsm_pdp;
	FreeSmartphoneDeviceIdleNotifier *idle_notifier;
	FreeSmartphoneDeviceInput *input;
	FreeSmartphoneDeviceDisplay *display;
	FreeSmartphoneDevicePowerSupply *power_supply;
	FreeSmartphonePIMMessages *pim_messages;
};
static struct _fso fso;

enum PhoneUiDialogType {
	PHONEUI_DIALOG_ERROR_DO_NOT_USE = 0,
	// This value is used for checking if we get a wrong pointer out of a HashTable.
	// So do not use it, and leave it first in this enum. ( because 0 == NULL )
	PHONEUI_DIALOG_MESSAGE_STORAGE_FULL,
	PHONEUI_DIALOG_SIM_NOT_PRESENT
};

typedef struct {
	int id;
} call_t;


static gboolean show_sim_not_present = TRUE;
static gboolean sim_ready = FALSE;
static gboolean gsm_request_running = FALSE;
static gboolean gsm_available = FALSE;
static gboolean func_is_set = FALSE;
static gboolean sim_check_needed = TRUE;
static time_t startup_time = 0;
static call_t *incoming_calls = NULL;
static call_t *outgoing_calls = NULL;
static int incoming_calls_size = 0;
static int outgoing_calls_size = 0;
static gboolean display_state = FALSE;


static gboolean _fso_list_resources();
static gboolean _fso_request_gsm();
static void _fso_suspend();
static void _stop_startup();
static void _startup_check();
static gint _fso_sim_info();


/* dbus method callbacks */
static void _list_resources_callback(GObject *source, GAsyncResult *res, gpointer data);
static void _request_resource_callback(GObject *source, GAsyncResult *res, gpointer data);
static void _going_offline_callback(GObject *source, GAsyncResult *res, gpointer data);
static void _going_online_callback(GObject *source, GAsyncResult *res, gpointer data);
static void _gsm_sim_ready_status_callback(GObject *source, GAsyncResult *res, gpointer data);
static void _gsm_sim_sim_info_callback(GObject *source, GAsyncResult *res, gpointer data);
static void _set_functionality_callback(GObject *source, GAsyncResult *res, gpointer data);
static void _get_power_status_callback(GObject *source, GAsyncResult *res, gpointer data);
static void _get_idle_state_callback(GObject *source, GAsyncResult *res, gpointer data);

/* dbus signal handlers */
static void _usage_resource_available_handler(GSource *source, char *resource, gboolean availability, gpointer data);
static void _usage_resource_changed_handler(GSource *source, char *resource, gboolean state, GHashTable *attributes, gpointer data);
static void _usage_system_action_handler(GSource* source, FreeSmartphoneUsageSystemAction action, gpointer data);
static void _gsm_sim_ready_status_handler(GSource *source, gboolean status, gpointer data);
static void _gsm_device_status_handler(GSource *source, FreeSmartphoneGSMDeviceStatus status, gpointer data);
static void _device_idle_notifier_state_handler(GSource *source, FreeSmartphoneDeviceIdleState state, gpointer data);
static void _device_input_event_handler(GSource *source, char *src, FreeSmartphoneDeviceInputState state, int duration, gpointer data);
static void _gsm_call_status_handler(GSource *source, int call_id, int status, GHashTable *properties, gpointer data);
static void _pim_incoming_message_handler(GSource *source, char *message_path, gpointer data);
static void _gsm_network_incoming_ussd_handler(GSource *source, int mode, char *message, gpointer data);
static void _gsm_network_status_handler(GSource *source, GHashTable *status, gpointer data);

/* call management */
static void _call_add(call_t ** calls, int *size, int id);
static int _call_check(call_t * calls, int *size, int id);
static void _call_remove(call_t ** calls, int *size, int id);


static gpointer
_dbus_proxy(GType type, const gchar *obj, const gchar *path, const gchar *iface)
{
	GError *error = NULL;
	gpointer ret;

	ret = g_initable_new(type, NULL, &error,
				"g-flags", G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
				"g-name", obj,
				"g-bus-type", G_BUS_TYPE_SYSTEM,
				"g-object-path", path,
				"g-interface-name", iface,
				NULL);

	if (error) {
		g_warning("failed to connect to %s: %s", path, error->message);
		g_error_free(error);
		return NULL;
	}

	return ret;
}

gboolean
fso_init()
{
	sim_auth_needed = FALSE;
	if (!offline_mode) {
		g_message("Inhibiting suspend during startup phase (max %ds)",
			  inhibit_suspend_on_startup_time);
		startup_time = time(NULL);
	}

	fso_connect_usage();
	fso_connect_gsm();
	fso_connect_pim();
	fso_connect_device();

	/* send fsogsmd a ping to dbus-activate it, if not running yet */
/*	if (fso.gsm_device)
		free_smartphone_gsm_device_get_device_status(fso.gsm_device, NULL, NULL);*/
	g_debug("Done connecting to FSO");

	return TRUE;
}

void
fso_connect_usage()
{
	g_debug("connecting to %s", FSO_USAGE_SERVICE);

	fso.usage = (FreeSmartphoneUsage *)
			(_dbus_proxy
				(FREE_SMARTPHONE_TYPE_USAGE_PROXY,
				 FSO_USAGE_SERVICE,
				 FSO_USAGE_PATH,
				 FSO_USAGE_IFACE)
			);

	if (fso.usage) {
		g_signal_connect(G_OBJECT(fso.usage), "resource-changed",
				G_CALLBACK(_usage_resource_changed_handler), NULL);
		g_signal_connect(G_OBJECT(fso.usage), "resource-available",
				G_CALLBACK(_usage_resource_available_handler), NULL);
		g_signal_connect(G_OBJECT(fso.usage), "system-action",
				 G_CALLBACK(_usage_system_action_handler), NULL);
		g_debug("Connected to FSO/Usage");
	}
}

void
fso_connect_gsm()
{
	g_debug("connecting to %s", FSO_GSM_SERVICE);

	fso.gsm_device = (FreeSmartphoneGSMDevice *)
				(_dbus_proxy
					(FREE_SMARTPHONE_GSM_TYPE_DEVICE_PROXY,
					 FSO_GSM_SERVICE,
					 FSO_GSM_DEVICE_PATH,
					 FSO_GSM_DEVICE_IFACE)
				);

	if (fso.gsm_device) {
		g_debug("Connected to FSO/GSM/Device");
		g_signal_connect(G_OBJECT(fso.gsm_device), "device-status",
				 G_CALLBACK(_gsm_device_status_handler), NULL);
	}

	fso.gsm_sim = (FreeSmartphoneGSMSIM *)
				(_dbus_proxy
					(FREE_SMARTPHONE_GSM_TYPE_SIM_PROXY,
					 FSO_GSM_SERVICE,
					 FSO_GSM_DEVICE_PATH,
					 FSO_GSM_SIM_IFACE)
				);

	fso.gsm_network = (FreeSmartphoneGSMNetwork *)
				(_dbus_proxy
					(FREE_SMARTPHONE_GSM_TYPE_NETWORK_PROXY,
					 FSO_GSM_SERVICE,
					 FSO_GSM_DEVICE_PATH,
					 FSO_GSM_NETWORK_IFACE)
				);
	if (fso.gsm_network) {
		g_signal_connect(G_OBJECT(fso.gsm_network), "status",
				G_CALLBACK(_gsm_network_status_handler), NULL);
		g_signal_connect(G_OBJECT(fso.gsm_network), "incoming-ussd",
				G_CALLBACK(_gsm_network_incoming_ussd_handler), NULL);
		g_debug("Connected to FSO/GSM/Network");
	}

	fso.gsm_pdp = (FreeSmartphoneGSMPDP *)
				(_dbus_proxy
					(FREE_SMARTPHONE_GSM_TYPE_PDP_PROXY,
					 FSO_GSM_SERVICE,
					 FSO_GSM_DEVICE_PATH,
					 FSO_GSM_PDP_IFACE)
				);

	fso.gsm_call = (FreeSmartphoneGSMCall *)
				(_dbus_proxy
					(FREE_SMARTPHONE_GSM_TYPE_CALL_PROXY,
					 FSO_GSM_SERVICE,
					 FSO_GSM_DEVICE_PATH,
					 FSO_GSM_CALL_IFACE)
				);
	if (fso.gsm_call) {
		g_signal_connect(G_OBJECT(fso.gsm_call), "call-status",
				G_CALLBACK(_gsm_call_status_handler), NULL);
		g_debug("Connected to FSO/GSM/Call");
	}

}

void
fso_connect_pim()
{
	g_debug("connecting to %s", FSO_PIM_SERVICE);

	fso.pim_messages = (FreeSmartphonePIMMessages *)
				(_dbus_proxy
					(FREE_SMARTPHONE_PIM_TYPE_MESSAGES_PROXY,
					 FSO_PIM_SERVICE,
					 FSO_PIM_MESSAGES_PATH,
					 FSO_PIM_MESSAGES_IFACE)
				);
	if (fso.pim_messages) {
		g_signal_connect(G_OBJECT(fso.pim_messages), "incoming-message",
				G_CALLBACK(_pim_incoming_message_handler), NULL);
		g_debug("Connected to FSO/PIM/Messages");
	}
}

void
fso_connect_device()
{
	g_debug("connecting to %s", FSO_DEVICE_SERVICE);

	fso.idle_notifier = (FreeSmartphoneDeviceIdleNotifier *)
				(_dbus_proxy
					(FREE_SMARTPHONE_DEVICE_TYPE_IDLE_NOTIFIER_PROXY,
					 FSO_DEVICE_SERVICE,
					 FSO_DEVICE_IDLE_NOTIFIER_PATH,
					 FSO_DEVICE_IDLE_NOTIFIER_IFACE)
				);
	if (fso.idle_notifier) {
		g_signal_connect(G_OBJECT(fso.idle_notifier), "state",
			G_CALLBACK(_device_idle_notifier_state_handler), NULL);
		g_debug("Connected to FSO/Device/IdleNotifier");
	}

	fso.input = (FreeSmartphoneDeviceInput *)
			(_dbus_proxy
				(FREE_SMARTPHONE_DEVICE_TYPE_INPUT_PROXY,
				 FSO_DEVICE_SERVICE,
				 FSO_DEVICE_INPUT_PATH,
				 FSO_DEVICE_INPUT_IFACE)
			);
	if (fso.input) {
		g_signal_connect(G_OBJECT(fso.input), "event",
				 G_CALLBACK(_device_input_event_handler), NULL);
		g_debug("Connected to FSO/Device/Input");
	}

	fso.display = (FreeSmartphoneDeviceDisplay *)
				(_dbus_proxy
					(FREE_SMARTPHONE_DEVICE_TYPE_DISPLAY_PROXY,
					 FSO_DEVICE_SERVICE,
					 FSO_DEVICE_DISPLAY_PATH,
					 FSO_DEVICE_DISPLAY_IFACE)
				);
	if (fso.display) {
		g_debug("Connected to FSO/Device/Display");
	}

	fso.power_supply = (FreeSmartphoneDevicePowerSupply *)
				(_dbus_proxy
					(FREE_SMARTPHONE_DEVICE_TYPE_POWER_SUPPLY_PROXY,
					 FSO_DEVICE_SERVICE,
					 FSO_DEVICE_POWER_SUPPLY_PATH,
					 FSO_DEVICE_POWER_SUPPLY_IFACE)
				);
	if (fso.power_supply) {
		g_debug("Connected to FSO/Device/PowerSupply");
	}
}

gboolean
fso_startup()
{
	g_debug("FSO starting up");

	/* we only have to list the resources when we did
	 * not yet handle it due to a resource available
	 * signal for the GSM resource */
	if (!gsm_request_running && !gsm_available) {
		_fso_list_resources();
	}

	fso_dimit(100, DIM_SCREEN_ALWAYS);

	return FALSE;
}

static void
_fso_dim_screen(int percent)
{
	int b = default_brightness * percent / 100;
	if (b > 100) {
		b = 100;
	}
	else if (b < minimum_brightness) {
		b = 0;
	}

	free_smartphone_device_display_set_brightness
			(fso.display, b, NULL, NULL);

	if (b == 0) {
		phoneui_idle_screen_call_activate_screensaver
			(phoneui.idle_screen, NULL,
			 phoneui_activate_screensaver_cb, NULL);
	}
	else {
		phoneui_idle_screen_call_deactivate_screensaver
			(phoneui.idle_screen, NULL,
			 phoneui_deactivate_screensaver_cb, NULL);
	}
}

static void
_get_power_status_for_dimming_callback(GObject *source,
					    GAsyncResult *res,
					    gpointer data)
{
	(void) source;
	GError *error = NULL;
	FreeSmartphoneDevicePowerStatus status;

	status = free_smartphone_device_power_supply_get_power_status_finish
						(fso.power_supply, res, &error);
	g_debug("PowerStatus is %d", status);
	if (error == NULL && (status == FREE_SMARTPHONE_DEVICE_POWER_STATUS_AC ||
		status == FREE_SMARTPHONE_DEVICE_POWER_STATUS_CHARGING)) {
		g_debug("not suspending due to charging or battery full");
		return;
	}
	_fso_dim_screen(GPOINTER_TO_INT(data));
}

void
fso_dimit(int percent, int dim)
{
	/* -1 means dimming disabled */
	if (dim == DIM_SCREEN_NEVER || percent < 0)
		return;

	/* for dimming only on bat we have to check
	 * if power is plugged in */
	if (dim == DIM_SCREEN_ONBAT) {
		free_smartphone_device_power_supply_get_power_status
			(fso.power_supply,
			_get_power_status_for_dimming_callback,
			 GINT_TO_POINTER(percent));
		return;
	}
	_fso_dim_screen(percent);
}

gboolean
fso_set_functionality()
{
	if (offline_mode) {
		_stop_startup();
		free_smartphone_gsm_device_set_functionality
			(fso.gsm_device, "airplane", FALSE, sim_pin ? sim_pin : "",
			_set_functionality_callback, NULL);
	}
	else {
		free_smartphone_gsm_device_set_functionality
			(fso.gsm_device, "full", TRUE, sim_pin ? sim_pin : "",
			_set_functionality_callback, NULL);
	}
	return FALSE;
}

void
fso_pdp_set_credentials()
{
	if (!pdp_apn || !pdp_user || !pdp_password)
		return;

	free_smartphone_gsm_pdp_set_credentials
		(fso.gsm_pdp, pdp_apn, pdp_user, pdp_password, NULL, NULL);
}

static gboolean
_fso_list_resources()
{
	free_smartphone_usage_list_resources(fso.usage,
			_list_resources_callback, NULL);
	return FALSE;
}

static gboolean
_fso_request_gsm()
{
	if (gsm_request_running) {
		/* do not request GSM twice */
		g_warning("GSM request still running...");
	}
	else if (gsm_available) {
		/* only request GSM if we know it is available */
		g_debug("Request GSM resource");
		gsm_request_running = TRUE;
		free_smartphone_usage_request_resource(fso.usage, "GSM",
			_request_resource_callback, NULL);
	}
	else {
		g_warning("Not requesting GSM as it is not available");
	}
	_startup_check();
	return FALSE;
}

static void
_fso_suspend(void)
{
	if (auto_suspend == SUSPEND_NEVER ||
			startup_time > 0 ||
			incoming_calls_size > 0 ||
			outgoing_calls_size > 0) {
		return;
	}

	/* for normal suspend behaviour we have to check
	 * if power is plugged in */
	if (auto_suspend == SUSPEND_NORMAL) {
		free_smartphone_device_power_supply_get_power_status
			(fso.power_supply,
			 _get_power_status_callback, NULL);
		return;
	}

	free_smartphone_usage_suspend(fso.usage, NULL, NULL);
}

static gint
_fso_sim_info()
{
	free_smartphone_gsm_sim_get_sim_info
		(fso.gsm_sim, _gsm_sim_sim_info_callback, NULL);
	return 0;
}


/* --- dbus callbacks --- */
static void
_list_resources_callback(GObject *source, GAsyncResult *res, gpointer data)
{
	char **resources;
	int count;
	GError *error = NULL;

	/* if we successfully got a list of resources...
	 * check if GSM is within them and request it if
	 * so, otherwise wait for ResourceAvailable signal */
	g_debug("list_resources_callback()");
	resources = free_smartphone_usage_list_resources_finish
			(fso.usage, res, &count, &error);
	_startup_check();
	if (error) {
		if (error->code == G_DBUS_ERROR_SERVICE_UNKNOWN) {
			g_critical("fsousaged not installed: %s", error->message);
		}
		else {
			g_message("error - retrying in 1s: (%d) %s", error->code, error->message);
			g_timeout_add(1000, _fso_list_resources, NULL);
		}
		g_error_free(error);
		return;
	}

	if (resources) {
		int i = 0;
		while (resources[i] != NULL) {
			g_debug("Resource %s available", resources[i]);
			if (!strcmp(resources[i], "GSM")) {
				gsm_available = TRUE;
				break;
			}
			i++;
		}

		if (gsm_available)
			_fso_request_gsm();
	}
}

static void
_request_resource_callback(GObject *source, GAsyncResult *res, gpointer data)
{
	(void) data;
	GError *error = NULL;

	g_debug("_request_resource_callback()");

	gsm_request_running = FALSE;
	_startup_check();

	free_smartphone_usage_request_resource_finish(fso.usage, res, &error);
	if (error == NULL) {
		/* nothing to do when there is no error
		 * the signal handler for ResourceChanged
		 * will do the rest */
		return;
	}

	if (error->domain == FREE_SMARTPHONE_USAGE_ERROR &&
		error->code == FREE_SMARTPHONE_USAGE_ERROR_USER_EXISTS) {
		g_message("we already requested GSM!!!");
		return;
	}

	/* we only request the GSM resource if it is actually
	 * available... if this does not work we retry it after
	 * some timeout ... */
	g_debug("request resource error, try again in 1s");
	g_debug("error: %s %s %d", error->message,
		g_quark_to_string(error->domain), error->code);
	g_timeout_add(1000, _fso_request_gsm, NULL);
}

static void
_set_functionality_callback(GObject *source, GAsyncResult *res, gpointer data)
{
	(void) data;
	GError *error = NULL;

	free_smartphone_gsm_device_set_functionality_finish(fso.gsm_device,
							    res, &error);
	if (error) {
		g_warning("SetFunctionality gave an error: %s", error->message);
		_startup_check();
		g_error_free(error);
		return;
	}
	func_is_set = TRUE;
}

static void
_get_power_status_callback(GObject *source, GAsyncResult *res, gpointer data)
{
	(void) source;
	(void) data;
	GError *error = NULL;
	FreeSmartphoneDevicePowerStatus status;

	status = free_smartphone_device_power_supply_get_power_status_finish
						(fso.power_supply, res, &error);
	g_debug("PowerStatus is %d", status);
	if (error == NULL && (status == FREE_SMARTPHONE_DEVICE_POWER_STATUS_AC ||
		status == FREE_SMARTPHONE_DEVICE_POWER_STATUS_CHARGING)) {
		g_debug("not suspending due to charging or battery full");
		return;
	}
	free_smartphone_usage_suspend(fso.usage, NULL, NULL);
}

static void
_get_idle_state_callback(GObject* source, GAsyncResult* res, gpointer data)
{
	(void) source;
	(void) data;
	GError *error = NULL;
	FreeSmartphoneDeviceIdleState state;

	state = free_smartphone_device_idle_notifier_get_state_finish
					(fso.idle_notifier, res, &error);
	if (error) {
		g_warning("IdleState error: (%d) %s", error->code, error->message);
		g_error_free(error);
		return;
	}
	g_debug("Current IdleState is %s",
		free_smartphone_device_idle_state_to_string(state));
	if (state == FREE_SMARTPHONE_DEVICE_IDLE_STATE_SUSPEND) {
		_fso_suspend();
	}
}

static void
_gsm_sim_sim_info_callback(GObject* source, GAsyncResult* res, gpointer data)
{
	(void) source;
	(void) data;
	GError *error = NULL;
	GHashTable *info;
	int slots_total = -1, slots_used = -1;
	GVariant *tmp;

	info = free_smartphone_gsm_sim_get_sim_info_finish(fso.gsm_sim, res, &error);
	if (error) {
		g_warning("Failed getting SIM info: (%d) %s",
			  error->code, error->message);
		g_error_free(error);
		return;
	}
	tmp = g_hash_table_lookup(info, "slots");
	if (tmp) {
		slots_total = g_variant_get_int32(tmp);
		g_debug("SimInfo has slots total = %d", slots_total);
	}
	tmp = g_hash_table_lookup(info, "used");
	if (tmp) {
		slots_used = g_variant_get_int32(tmp);
		g_debug("SimInfo has slots used = %d", slots_used);
	}
	if (slots_total == -1 || slots_used == -1) {
		g_debug("SimInfo has no slots and/or used properties - retrying later");
		g_timeout_add_seconds(3, _fso_sim_info, NULL);
	}
	else {
		sim_check_needed = FALSE;
		if (slots_total - slots_used < MIN_SIM_SLOTS_FREE) {
			g_message("No more free slots for messages on SIM!");
			// TODO: verify if one free slot is needed for receiving
			//       messages and remove this dialog if not
			phoneui_notification_call_display_dialog
				(phoneui.notification, PHONEUI_DIALOG_MESSAGE_STORAGE_FULL,
				 NULL, phoneui_show_dialog_cb, NULL);
		}
		else {
			g_debug("SIM has %d free slots for messages",
				slots_total - slots_used);
		}
	}

	g_hash_table_unref(info);
}


/* dbus signal handlers */
static void
_usage_resource_available_handler(GSource *source, char *name,
			    gboolean availability, gpointer data)
{
	(void) source;
	(void) data;
	g_debug("resource %s is now %s", name,
		availability ? "available" : "vanished");
	if (strcmp(name, "GSM") == 0) {
		gsm_available = availability;
		if (gsm_available) {
			_fso_request_gsm();
		}
		else {
			gsm_request_running = FALSE;
		}
	}
}

static void
_gsm_device_status_callback(GObject* source, GAsyncResult* res, gpointer data)
{
	GError *error = NULL;
	FreeSmartphoneGSMDeviceStatus status;

	status = free_smartphone_gsm_device_get_device_status_finish
				((FreeSmartphoneGSMDevice *)source, res, &error);
	if (error) {
		g_warning("%d: %s", error->code, error->message);
		g_error_free(error);
		return;
	}

	_gsm_device_status_handler(NULL, status, NULL);
}

static void
_usage_resource_changed_handler(GSource *source, char *name, gboolean state,
			  GHashTable * attributes, gpointer data)
{
	(void) source;
	(void) data;
	GVariant *tmp;
	g_debug("resource %s is now %s", name, state ? "enabled" : "disabled");
	tmp = g_hash_table_lookup(attributes, "policy");
	if (tmp)
		g_debug("   policy:   %s", g_variant_get_string(tmp, NULL));
	tmp = g_hash_table_lookup(attributes, "refcount");
	if (tmp)
		g_debug("   refcount: %d", g_variant_get_int32(tmp));

	if (strcmp(name, "Display") == 0) {
		g_debug("Display state state changed: %s",
			state ? "enabled" : "disabled");
		display_state = state;
		/* if something requests the Display resource
		we have * to undim it */
		if (display_state) {
			fso_dimit(100, DIM_SCREEN_ALWAYS);
		}
		return;
	}

	if (strcmp(name, "GSM") == 0) {
		free_smartphone_gsm_device_get_device_status
				(fso.gsm_device, _gsm_device_status_callback, NULL);
		return;
	}
}

static void
_usage_system_action_handler(GSource *source,
			     FreeSmartphoneUsageSystemAction action,
			     gpointer data)
{
	g_debug("SystemAction: %d", action);
	/* show the IdleScreen if configured to do so on suspend */
	if (action == FREE_SMARTPHONE_USAGE_SYSTEM_ACTION_SUSPEND &&
		idle_screen & IDLE_SCREEN_SUSPEND)  {
		phoneui_idle_screen_call_display
			(phoneui.idle_screen, NULL, phoneui_show_idle_cb, NULL);
	}
}

static void
_device_idle_notifier_state_handler(GSource *source,
				    FreeSmartphoneDeviceIdleState state,
				    gpointer data)
{
	(void) source;
	(void) data;

	/* while Display resource is requested nothing to do */
	if (display_state && state != FREE_SMARTPHONE_DEVICE_IDLE_STATE_BUSY) {
		g_debug("Not handling Idle while Display is requested");
		return;
	}
	switch (state) {
	case FREE_SMARTPHONE_DEVICE_IDLE_STATE_BUSY:
		fso_dimit(100, dim_screen);
		break;
	case FREE_SMARTPHONE_DEVICE_IDLE_STATE_IDLE:
		fso_dimit(dim_idle_percent, dim_screen);
		break;
	case FREE_SMARTPHONE_DEVICE_IDLE_STATE_IDLE_DIM:
		fso_dimit(dim_idle_dim_percent, dim_screen);
		break;
	case FREE_SMARTPHONE_DEVICE_IDLE_STATE_IDLE_PRELOCK:
		fso_dimit(dim_idle_prelock_percent, dim_screen);
		break;
	case FREE_SMARTPHONE_DEVICE_IDLE_STATE_LOCK:
		if (idle_screen & IDLE_SCREEN_LOCK &&
				((idle_screen & IDLE_SCREEN_PHONE) ||
				 ((incoming_calls_size == 0) &&
				 (outgoing_calls_size == 0)))) {
			phoneui_idle_screen_call_display
				(phoneui.idle_screen, NULL,
				 phoneui_show_idle_cb, NULL);
		}
		break;
	case FREE_SMARTPHONE_DEVICE_IDLE_STATE_SUSPEND:
		_fso_suspend();
		break;
	}
}

static void
_device_input_event_handler(GSource *source, char *src,
			    FreeSmartphoneDeviceInputState state,
			    int duration, gpointer data)
{
	(void) source;
	(void) data;
	g_debug("INPUT EVENT: %s - %d - %d", src, state, duration);
	if (idle_screen & IDLE_SCREEN_AUX &&
		!strcmp(src, "AUX") &&
		state == FREE_SMARTPHONE_DEVICE_INPUT_STATE_RELEASED) {
		phoneui_idle_screen_call_toggle(phoneui.idle_screen, NULL,
						    phoneui_toggle_idle_cb, NULL);
	}
}

static void
_gsm_call_status_handler(GSource *source, int call_id, int status,
		     GHashTable *properties, gpointer data)
{
	g_debug("call status handler called, id: %d, status: %d", call_id,
		status);

	GVariant *peerNumber = g_hash_table_lookup(properties, "peer");
	gchar *number;
	if (peerNumber != NULL) {
		gsize len;
		number = g_variant_dup_string(peerNumber, &len);
		/* FIXME: fix the ugly " bug
		 * we potentially waste a couple of bytes, fix it in a normal manner*/
		if (number[0] == '"') {
			gchar *tmp;
			tmp = strdup(number);
			strcpy(number, &tmp[1]);
			len--;
		}
		if (len > 0 && number[len - 1] == '"') {
			number[len - 1] = '\0';
		}

	}
	else {
		number = "*****";
	}

	switch (status) {
		case FREE_SMARTPHONE_GSM_CALL_STATUS_INCOMING:
			g_debug("incoming call");
			if (_call_check(incoming_calls,
					&incoming_calls_size, call_id) == -1) {
				_call_add(&incoming_calls,
					&incoming_calls_size, call_id);
				fso_dimit(100, DIM_SCREEN_ALWAYS);
				free_smartphone_usage_request_resource
					(fso.usage, "CPU", NULL, NULL);
				phoneui_call_management_call_display_incoming
					(phoneui.call_management,
					call_id, status, number,
					NULL, phoneui_show_incoming_cb, NULL);
			}
			break;
		case FREE_SMARTPHONE_GSM_CALL_STATUS_OUTGOING:
			g_debug("outgoing call");
			if (_call_check(outgoing_calls,
					&outgoing_calls_size, call_id) == -1) {
				_call_add(&outgoing_calls,
					&outgoing_calls_size, call_id);
				free_smartphone_usage_request_resource
					(fso.usage, "CPU", NULL, NULL);
				phoneui_call_management_call_display_outgoing
					(phoneui.call_management,
					call_id, status, number,
					NULL, phoneui_show_outgoing_cb, NULL);
			}
			break;
		case FREE_SMARTPHONE_GSM_CALL_STATUS_RELEASE:
			g_debug("release call");
			if (_call_check(incoming_calls,
					&incoming_calls_size, call_id) != -1) {
				_call_remove(&incoming_calls,
						&incoming_calls_size, call_id);
				phoneui_call_management_call_hide_incoming
					(phoneui.call_management,
					 call_id,
					 NULL, phoneui_hide_incoming_cb, NULL);
			}
			if (_call_check(outgoing_calls,
					&outgoing_calls_size, call_id) != -1) {
				_call_remove(&outgoing_calls,
						&outgoing_calls_size, call_id);
				phoneui_call_management_call_hide_outgoing
					(phoneui.call_management, call_id,
					 NULL, phoneui_hide_outgoing_cb, NULL);
			}
			if (incoming_calls_size == 0 && outgoing_calls_size == 0) {
				free_smartphone_usage_release_resource
					(fso.usage, "CPU", NULL, NULL);
			}
			break;
		case FREE_SMARTPHONE_GSM_CALL_STATUS_HELD:
			g_debug("held call");
			break;
		case FREE_SMARTPHONE_GSM_CALL_STATUS_ACTIVE:
			g_debug("active call");
			break;
		default:
			g_debug("Unknown CallStatus");
			break;
	}
}

static void
_gsm_device_status_handler(GSource *source,
			   FreeSmartphoneGSMDeviceStatus status,
			   gpointer data)
{
	(void) data;
	(void) source;
	g_debug("_gsm_device_status_handler: status=%s",
		 free_smartphone_gsm_device_status_to_string(status));
	if (status == FREE_SMARTPHONE_GSM_DEVICE_STATUS_ALIVE_NO_SIM) {
		if (show_sim_not_present)
		{
			phoneui_notification_call_display_dialog
				(phoneui.notification, PHONEUI_DIALOG_SIM_NOT_PRESENT,
				NULL, phoneui_show_dialog_cb, NULL);
			show_sim_not_present = FALSE;
		}
	}
	else if (status == FREE_SMARTPHONE_GSM_DEVICE_STATUS_ALIVE_SIM_LOCKED) {
		if (sim_pin) {
			fso_set_functionality();
		}
		else {
			g_debug("SIM auth needed... showing PIN dialog");
			sim_auth_needed = TRUE;
			phoneui_notification_call_display_sim_auth
				(phoneui.notification, status, NULL,
				 phoneui_show_sim_auth_cb, NULL);
		}
	}
	else if (status == FREE_SMARTPHONE_GSM_DEVICE_STATUS_ALIVE_SIM_READY) {
		g_debug("SIM is alive-sim-ready");
		sim_auth_needed = FALSE;
		if (!func_is_set) {
			fso_set_functionality();
		}
		if (sim_check_needed) {
			g_timeout_add_seconds(2, _fso_sim_info, NULL);
		}
	}
	else if (status == FREE_SMARTPHONE_GSM_DEVICE_STATUS_ALIVE_REGISTERED) {
		g_debug("alive-registered");
		fso_pdp_set_credentials();
		free_smartphone_gsm_network_set_calling_identification
			(fso.gsm_network, calling_identification, NULL, NULL);
	}
}

static void
_pim_incoming_message_handler(GSource *source, char *message_path, gpointer data)
{
	(void) source;
	(void) data;
	g_debug("fso_incoming_message_handler(%s)", message_path);
	if (show_incoming_sms) {
		phoneui_messages_call_display_message
			(phoneui.messages, message_path, NULL,
			 phoneui_display_message_cb, NULL);
	}
	/* check if there is still a free slot for the next SMS */
	free_smartphone_gsm_sim_get_sim_info
				(fso.gsm_sim, _gsm_sim_sim_info_callback, NULL);
}

static void
_gsm_network_incoming_ussd_handler(GSource *source, int mode,
				   char *message, gpointer data)
{
	(void) source;
	(void) data;
	g_debug("fso_incoming_ussd_handler(mode=%d, message=%s)", mode,
		message);
	phoneui_notification_call_display_ussd
		(phoneui.notification, mode, message, NULL,
		 phoneui_show_ussd_cb, NULL);
}

static void
_gsm_network_status_handler(GSource *source, GHashTable *status, gpointer data)
{
	(void) source;
	(void) data;

	if (!status) {
		g_debug("got no status from NetworkStatus?!");
		return;
	}

	/* right now we use this signal only to check if it registered on startup
	to reset the startup time... nothing to do if it is already reset */
	if (startup_time == -1) {
		return;
	}

	GVariant *tmp = g_hash_table_lookup(status, "registration");
	if (tmp) {
		const char *registration = g_variant_get_string(tmp, NULL);
		g_debug("fso_network_status_handler(registration=%s)",
				registration);
		if (strcmp(registration, "unregistered")) {
			g_message("Ending startup phase due to successfull registration");
			_stop_startup();
			return;
		}
	}
	else {
		g_debug("got NetworkStatus without registration?!?");
	}

	_startup_check();
}

static void
_stop_startup()
{
	startup_time = -1;

	/* we have to check the current idle state... and if it is suspend
	then we have to suspend... otherwise it would never suspend without
	touching the screen */
	g_debug("Getting current IdleState to see if we have to suspend");
	free_smartphone_device_idle_notifier_get_state(fso.idle_notifier,
						_get_idle_state_callback, NULL);
}

static void
_startup_check()
{
	time_t now;

	if (startup_time == -1) {
		return;
	}

	now = time(NULL);
	if (now - startup_time > inhibit_suspend_on_startup_time) {
		g_message("Ending startup phase due to time out");
		_stop_startup();
	}
}

/* call management */
static void
_call_add(call_t ** calls, int *size, int id)
{
	g_debug("_call_add(%d)", id);
	(*size)++;
	if (*size == 1)
		*calls = malloc(sizeof(call_t));
	else
		*calls = realloc(*calls, sizeof(call_t) * (*size));
	(*calls)[(*size) - 1].id = id;
}

static int
_call_check(call_t * calls, int *size, int id)
{
	int i;
	g_debug("_call_check(%d)", id);
	for (i = 0; i < (*size); i++) {
		if (calls[i].id == id)
			return i;
	}
	return -1;
}

static void
_call_remove(call_t ** calls, int *size, int id)
{
	g_debug("_call_remove(%d)", id);
	if (*size == 1) {
		free(*calls);
		(*size)--;
		*calls = NULL;
	}
	else {
		int place = _call_check(*calls, size, id);
		if (place >= 0) {
			int i = place;
			for (i = place; i + 1 < (*size); i++) {
				(*calls)[i].id = (*calls)[i + 1].id;
			}
			(*size)--;
			*calls = realloc(*calls, sizeof(call_t) * (*size));
		}
	}
}
