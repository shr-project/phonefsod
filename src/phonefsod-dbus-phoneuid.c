
#include <string.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-bindings.h>
#include "phonefsod-dbus-common.h"
#include "phonefsod-dbus-phoneuid.h"
#include "phonefsod-globals.h"


static void
_dbus_error(GError *error)
{
	if (error->domain == DBUS_GERROR &&
		error->code == DBUS_GERROR_REMOTE_EXCEPTION) {
		g_message("Remote method exception %s: %s",
			dbus_g_error_get_name (error),
			error->message);
	}
	else {
		g_message("Error: %s", error->message);
	}
	g_error_free(error);
}

static DBusGProxy *
_dbus_get_proxy(const char *name, const char *path, const char *interface)
{
	DBusGProxy *proxy = dbus_g_proxy_new_for_name(system_bus,
			name, path, interface);
	return (proxy);
}

/* --- org.shr.phoneui.CallManagement --- */
void phoneuid_call_management_show_incoming(int callid, int status,
		const char *number)
{
	DBusGProxy *proxy = _dbus_get_proxy
			(PHONEUID_CALL_MANAGEMENT_NAME,
			PHONEUID_CALL_MANAGEMENT_PATH,
			PHONEUID_CALL_MANAGEMENT_INTERFACE);
	if (proxy) {
		GError *error = NULL;
		dbus_g_proxy_call (proxy, "DisplayIncoming", &error,
			G_TYPE_INT, callid, G_TYPE_INT, status,
			G_TYPE_STRING, number, G_TYPE_INVALID,
			G_TYPE_INVALID);
		if (error)
			_dbus_error(error);
	}
}

void phoneuid_call_management_hide_incoming(int callid)
{
	DBusGProxy *proxy = _dbus_get_proxy
			(PHONEUID_CALL_MANAGEMENT_NAME,
			PHONEUID_CALL_MANAGEMENT_PATH,
			PHONEUID_CALL_MANAGEMENT_INTERFACE);
	if (proxy) {
		GError *error = NULL;
		dbus_g_proxy_call (proxy, "HideIncoming", &error,
			G_TYPE_INT, callid, G_TYPE_INVALID,
			G_TYPE_INVALID);
		if (error)
			_dbus_error(error);
	}
}

void
phoneuid_call_management_show_outgoing(int callid, int status,
		const char *number)
{
	DBusGProxy *proxy = _dbus_get_proxy
			(PHONEUID_CALL_MANAGEMENT_NAME,
			PHONEUID_CALL_MANAGEMENT_PATH,
			PHONEUID_CALL_MANAGEMENT_INTERFACE);
	if (proxy) {
		GError *error = NULL;
		dbus_g_proxy_call (proxy, "DisplayOutgoing", &error,
			G_TYPE_INT, callid, G_TYPE_INT, status,
			G_TYPE_STRING, number, G_TYPE_INVALID,
			G_TYPE_INVALID);
		if (error)
			_dbus_error(error);
	}
}

void
phoneuid_call_management_hide_outgoing(int callid)
{
	DBusGProxy *proxy = _dbus_get_proxy
			(PHONEUID_CALL_MANAGEMENT_NAME,
			PHONEUID_CALL_MANAGEMENT_PATH,
			PHONEUID_CALL_MANAGEMENT_INTERFACE);
	if (proxy) {
		GError *error = NULL;
		dbus_g_proxy_call (proxy, "HideOutgoing", &error,
			G_TYPE_INT, callid, G_TYPE_INVALID,
			G_TYPE_INVALID);
		if (error)
			_dbus_error(error);
	}
}

/* --- org.shr.phoneui.Messages --- */
void
phoneuid_messages_display_message(const char *message_path)
{
	DBusGProxy *proxy = _dbus_get_proxy
			(PHONEUID_MESSAGES_NAME,
			PHONEUID_MESSAGES_PATH,
			PHONEUID_MESSAGES_INTERFACE);
	if (proxy) {
		GError *error = NULL;
		dbus_g_proxy_call (proxy, "DisplayMessage", &error,
			G_TYPE_STRING, message_path, G_TYPE_INVALID,
			G_TYPE_INVALID);
		if (error)
			_dbus_error(error);
	}
}

/* --- org.shr.phoneui.Notification --- */
void
phoneuid_notification_show_dialog(int dialog)
{
	DBusGProxy *proxy = _dbus_get_proxy
			(PHONEUID_NOTIFICATION_NAME,
			PHONEUID_NOTIFICATION_PATH,
			PHONEUID_NOTIFICATION_INTERFACE);
	if (proxy) {
		GError *error = NULL;
		dbus_g_proxy_call(proxy, "DisplayDialog", &error,
				G_TYPE_INT, dialog, G_TYPE_INVALID,
				G_TYPE_INVALID);
		if (error)
			_dbus_error(error);
	}
}

void
phoneuid_notification_show_sim_auth(int status)
{
	DBusGProxy *proxy = _dbus_get_proxy
			(PHONEUID_NOTIFICATION_NAME,
			PHONEUID_NOTIFICATION_PATH,
			PHONEUID_NOTIFICATION_INTERFACE);
	if (proxy) {
		GError *error = NULL;
		dbus_g_proxy_call(proxy, "DisplaySimAuth", &error,
				G_TYPE_INT, status, G_TYPE_INVALID,
				G_TYPE_INVALID);
		if (error)
			_dbus_error(error);
	}
}

void
phoneuid_notification_hide_sim_auth(int status)
{
	DBusGProxy *proxy = _dbus_get_proxy
			(PHONEUID_NOTIFICATION_NAME,
			PHONEUID_NOTIFICATION_PATH,
			PHONEUID_NOTIFICATION_INTERFACE);
	if (proxy) {
		GError *error = NULL;
		dbus_g_proxy_call(proxy, "HideSimAuth", &error,
				G_TYPE_INT, status, G_TYPE_INVALID,
				G_TYPE_INVALID);
		if (error)
			_dbus_error(error);
	}
}

void
phoneuid_notification_show_ussd(int mode, const char *message)
{
	DBusGProxy *proxy = _dbus_get_proxy
			(PHONEUID_NOTIFICATION_NAME,
			PHONEUID_NOTIFICATION_PATH,
			PHONEUID_NOTIFICATION_INTERFACE);
	if (proxy) {
		GError *error = NULL;
		dbus_g_proxy_call(proxy, "DisplayUssd", &error,
				G_TYPE_INT, mode,
				G_TYPE_STRING, message,
				G_TYPE_INVALID, G_TYPE_INVALID);
		if (error)
			_dbus_error(error);
	}
}


