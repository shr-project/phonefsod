/*
 *  Copyright (C) 2009
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

#ifndef _PHONEFSOD_DBUS_PHONEUID_H
#define _PHONEFSOD_DBUS_PHONEUID_H

/* --- org.shr.phoneuid.CallManagement --- */
void phoneuid_call_management_show_incoming(int callid, int status, const char *number);
void phoneuid_call_management_hide_incoming(int callid);
void phoneuid_call_management_show_outgoing(int callid, int status, const char *number);
void phoneuid_call_management_hide_outgoing(int callid);

/* --- org.shr.phoneuid.Messages --- */
void phoneuid_messages_display_message(const char *message_path);

/* --- org.shr.phoneuid.Dialogs --- */
void phoneuid_notification_show_dialog(int dialog);
void phoneuid_notification_show_sim_auth(int status);
void phoneuid_notification_hide_sim_auth(int status);
void phoneuid_notification_show_ussd(int mode, const char *message);

void phoneuid_idle_screen_show();
void phoneuid_idle_screen_hide();
void phoneuid_idle_screen_toggle();
void phoneuid_idle_screen_activate_screensaver();
void phoneuid_idle_screen_deactivate_screensaver();

#endif
