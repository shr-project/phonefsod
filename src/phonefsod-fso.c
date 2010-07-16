
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <glib.h>
#include <glib/gthread.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus-glib-bindings.h>
#include <freesmartphone.h>
#include <fsoframework.h>
//#include <phoneui/phoneui.h>
#include "phonefsod-dbus-phoneuid.h"
#include "phonefsod-fso.h"
#include "phonefsod-globals.h"

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


static gboolean sim_ready = FALSE;
static gboolean gsm_request_running = FALSE;
static gboolean gsm_available = FALSE;
static time_t startup_time = FALSE;
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

/* dbus method callbacks */
static void _list_resources_callback(GSource *source, GAsyncResult *res, gpointer data);
static void _request_resource_callback(GSource *source, GAsyncResult *res, gpointer data);
static void _going_offline_callback(GSource *source, GAsyncResult *res, gpointer data);
static void _going_online_callback(GSource *source, GAsyncResult *res, gpointer data);
static void _gsm_sim_ready_status_callback(GSource *source, GAsyncResult *res, gpointer data);
static void _gsm_sim_sim_info_callback(GObject *source, GAsyncResult *res, gpointer data);
static void _set_functionality_callback(GSource *source, GAsyncResult *res, gpointer data);
static void _get_power_status_callback(GSource *source, GAsyncResult *res, gpointer data);
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

gboolean
fso_init()
{
	sim_auth_needed = FALSE;

	fso_connect_usage();
	fso_connect_gsm();
	fso_connect_pim();
	fso_connect_device();

	/* send fsogsmd a ping to dbus-activate it, if not running yet */
	free_smartphone_gsm_device_get_device_status(fso.gsm_device, NULL, NULL);
	g_debug("Done connecting to FSO");

	return TRUE;
}

void
fso_connect_usage()
{
	fso.usage = free_smartphone_get_usage_proxy(system_bus,
				FSO_FRAMEWORK_USAGE_ServiceDBusName,
				FSO_FRAMEWORK_USAGE_ServicePathPrefix);
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
	fso.gsm_device = free_smartphone_gsm_get_device_proxy(system_bus,
				FSO_FRAMEWORK_GSM_ServiceDBusName,
				FSO_FRAMEWORK_GSM_DeviceServicePath);
	if (fso.gsm_device) {
		g_debug("Connected to FSO/GSM/Device");
		g_signal_connect(fso.gsm_device, "device-status",
				 G_CALLBACK(_gsm_device_status_handler), NULL);
	}

	fso.gsm_sim = free_smartphone_gsm_get_s_i_m_proxy(system_bus,
				FSO_FRAMEWORK_GSM_ServiceDBusName,
				FSO_FRAMEWORK_GSM_DeviceServicePath);

	fso.gsm_network = free_smartphone_gsm_get_network_proxy(system_bus,
				FSO_FRAMEWORK_GSM_ServiceDBusName,
				FSO_FRAMEWORK_GSM_DeviceServicePath);
	if (fso.gsm_network) {
		g_signal_connect(G_OBJECT(fso.gsm_network), "status",
				G_CALLBACK(_gsm_network_status_handler), NULL);
		g_signal_connect(G_OBJECT(fso.gsm_network), "incoming-ussd",
				G_CALLBACK(_gsm_network_incoming_ussd_handler), NULL);
		g_debug("Connected to FSO/GSM/Network");
	}

	fso.gsm_pdp = free_smartphone_gsm_get_p_d_p_proxy(system_bus,
				FSO_FRAMEWORK_GSM_ServiceDBusName,
				FSO_FRAMEWORK_GSM_DeviceServicePath);

	fso.gsm_call = free_smartphone_gsm_get_call_proxy(system_bus,
				FSO_FRAMEWORK_GSM_ServiceDBusName,
				FSO_FRAMEWORK_GSM_DeviceServicePath);
	if (fso.gsm_call) {
		g_signal_connect(G_OBJECT(fso.gsm_call), "call-status",
				G_CALLBACK(_gsm_call_status_handler), NULL);
		g_debug("Connected to FSO/GSM/Call");
	}

}

void
fso_connect_pim()
{
	fso.pim_messages = free_smartphone_pim_get_messages_proxy(system_bus,
				FSO_FRAMEWORK_PIM_ServiceDBusName,
				FSO_FRAMEWORK_PIM_MessagesServicePath);
	if (fso.pim_messages) {
		g_signal_connect(G_OBJECT(fso.pim_messages), "incoming-message",
				G_CALLBACK(_pim_incoming_message_handler), NULL);
		g_debug("Connected to FSO/PIM/Messages");
	}
}

void
fso_connect_device()
{
	fso.idle_notifier = free_smartphone_device_get_idle_notifier_proxy
			(system_bus, FSO_FRAMEWORK_DEVICE_ServiceDBusName,
			FSO_FRAMEWORK_DEVICE_IdleNotifierServicePath "/0");
	if (fso.idle_notifier) {
		g_signal_connect(G_OBJECT(fso.idle_notifier), "state",
			G_CALLBACK(_device_idle_notifier_state_handler), NULL);
		g_debug("Connected to FSO/Device/IdleNotifier");
	}

	fso.input = free_smartphone_device_get_input_proxy(system_bus,
				FSO_FRAMEWORK_DEVICE_ServiceDBusName,
				FSO_FRAMEWORK_DEVICE_InputServicePath);
	if (fso.input) {
		g_signal_connect(G_OBJECT(fso.input), "event",
				 G_CALLBACK(_device_input_event_handler), NULL);
		g_debug("Connected to FSO/Device/Input");
	}

	fso.display = free_smartphone_device_get_display_proxy(system_bus,
				FSO_FRAMEWORK_DEVICE_ServiceDBusName,
				FSO_FRAMEWORK_DEVICE_DisplayServicePath "/0");
	if (fso.display) {
		g_debug("Connected to FSO/Device/Display");
	}

	fso.power_supply = free_smartphone_device_get_power_supply_proxy
				(system_bus,
				 FSO_FRAMEWORK_DEVICE_ServiceDBusName,
				 FSO_FRAMEWORK_DEVICE_PowerSupplyServicePath);
	if (fso.power_supply) {
		g_debug("Connected to FSO/Device/PowerSupply");
	}
}

gboolean
fso_startup()
{
	g_debug("FSO starting up");
	if (!offline_mode) {
		g_message("Inhibiting suspend during startup phase (max %ds)",
			  inhibit_suspend_on_startup_time);
		startup_time = time(NULL);
	}
	_fso_list_resources();
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
		phoneuid_idle_screen_activate_screensaver();
	}
	else {
		phoneuid_idle_screen_deactivate_screensaver();
	}
}

static void
_get_power_status_for_dimming_callback(GSource *source, GAsyncResult *res,
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
			(fso.gsm_device, "airplane", FALSE, "",
			(GAsyncReadyCallback)_set_functionality_callback, NULL);
	}
	else {
		free_smartphone_gsm_device_set_functionality
			(fso.gsm_device, "full", TRUE, "",
			(GAsyncReadyCallback)_set_functionality_callback, NULL);
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
			(GAsyncReadyCallback)_list_resources_callback, NULL);
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
			(GAsyncReadyCallback)_request_resource_callback, NULL);
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
			 (GAsyncReadyCallback)_get_power_status_callback, NULL);
		return;
	}

	free_smartphone_usage_suspend(fso.usage, NULL, NULL);
}

/* --- dbus callbacks --- */
static void
_list_resources_callback(GSource *source, GAsyncResult *res, gpointer data)
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
		g_message("  error: (%d) %s", error->code, error->message);
		g_timeout_add(1000, _fso_list_resources, NULL);
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
_request_resource_callback(GSource *source, GAsyncResult *res, gpointer data)
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
_set_functionality_callback(GSource *source, GAsyncResult *res, gpointer data)
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
}

static void
_get_power_status_callback(GSource *source, GAsyncResult *res, gpointer data)
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
	int slots_total = 0, slots_used = 0;
	GValue *gval_tmp;

	info = free_smartphone_gsm_sim_get_sim_info_finish(fso.gsm_sim, res, &error);
	if (error) {
		g_warning("Failed getting SIM info: (%d) %s",
			  error->code, error->message);
		g_error_free(error);
		return;
	}
	gval_tmp = g_hash_table_lookup(info, "slots");
	if (gval_tmp) {
		slots_total = g_value_get_int(gval_tmp);
	}
	gval_tmp = g_hash_table_lookup(info, "used");
	if (gval_tmp) {
		slots_used = g_value_get_int(gval_tmp);
	}
	if (slots_total - slots_used < MIN_SIM_SLOTS_FREE) {
		g_message("No more free slots for messages on SIM!");
		phoneuid_notification_show_dialog
					(PHONEUI_DIALOG_MESSAGE_STORAGE_FULL);
	}
	else {
		g_debug("SIM has %d free slots for messages",
			slots_total - slots_used);
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
		if (gsm_available)
			_fso_request_gsm();
	}
}

static void
_usage_resource_changed_handler(GSource *source, char *name, gboolean state,
			  GHashTable * attributes, gpointer data)
{
	(void) source;
	(void) data;
	gpointer p = NULL;
	g_debug("resource %s is now %s", name, state ? "enabled" : "disabled");
	p = g_hash_table_lookup(attributes, "policy");
	if (p)
		g_debug("   policy:   %d", g_value_get_int(p));
	p = g_hash_table_lookup(attributes, "refcount");
	if (p)
		g_debug("   refcount: %d", g_value_get_int(p));

	if (strcmp(name, "Display") == 0) {
		g_debug("Display state state changed: %s",
			state ? "enabled" : "disabled");
		display_state = state;
		/* if something requests the Display resource
		we have * to undim it */
		if (display_state) {
			fso_dimit(100, DIM_SCREEN_ALWAYS);
		}
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
		phoneuid_idle_screen_show();
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
		       phoneuid_idle_screen_show();
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
		phoneuid_idle_screen_toggle();
	}
}

static void
_gsm_call_status_handler(GSource *source, int call_id, int status,
		     GHashTable *properties, gpointer data)
{
	g_debug("call status handler called, id: %d, status: %d", call_id,
		status);

	GValue *peerNumber = g_hash_table_lookup(properties, "peer");
	gchar *number;
	if (peerNumber != NULL) {
		int len;
		number = g_strdup_value_contents(peerNumber);
		len = strlen(number);
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
				phoneuid_call_management_show_incoming(
						call_id, status, number);
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
				phoneuid_call_management_show_outgoing(
						call_id, status, number);
			}
			break;
		case FREE_SMARTPHONE_GSM_CALL_STATUS_RELEASE:
			g_debug("release call");
			if (_call_check(incoming_calls,
					&incoming_calls_size, call_id) != -1) {
				_call_remove(&incoming_calls,
						&incoming_calls_size, call_id);
				phoneuid_call_management_hide_incoming(
						call_id);
			}
			if (_call_check(outgoing_calls,
					&outgoing_calls_size, call_id) != -1) {
				_call_remove(&outgoing_calls,
						&outgoing_calls_size, call_id);
				phoneuid_call_management_hide_outgoing(
						call_id);
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
	if (status == FREE_SMARTPHONE_GSM_DEVICE_STATUS_ALIVE_NO_SIM) {
		phoneuid_notification_show_dialog(PHONEUI_DIALOG_SIM_NOT_PRESENT);
	}
	else if (status == FREE_SMARTPHONE_GSM_DEVICE_STATUS_ALIVE_SIM_LOCKED) {
		sim_auth_needed = TRUE;
		phoneuid_notification_show_sim_auth(status);
	}
	else if (status == FREE_SMARTPHONE_GSM_DEVICE_STATUS_ALIVE_SIM_READY) {
		g_debug("SIM is alive-sim-ready");
		sim_auth_needed = FALSE;
		fso_set_functionality();
		fso_pdp_set_credentials();
		free_smartphone_gsm_network_set_calling_identification
			(fso.gsm_network, calling_identification, NULL, NULL);
		free_smartphone_gsm_sim_get_sim_info
			(fso.gsm_sim, _gsm_sim_sim_info_callback, NULL);
	}
}

static void
_pim_incoming_message_handler(GSource *source, char *message_path, gpointer data)
{
	(void) source;
	(void) data;
	g_debug("fso_incoming_message_handler(%s)", message_path);
	if (show_incoming_sms) {
		phoneuid_messages_display_message(message_path);
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
	phoneuid_notification_show_ussd(mode, message);
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

	GValue *tmp = g_hash_table_lookup(status, "registration");
	if (tmp) {
		const char *registration = g_value_get_string(tmp);
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
