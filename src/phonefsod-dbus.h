/*
 *  Copyright (C) 2009-2012
 *      Authors (alphabetical) :
 *              Klaus 'mrmoku' Kurzmann <mok@fluxnetz.de>
 *              Lukas 'slyon' Märdian <luk@slyon.de>
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

#ifndef _PHONEFSOD_DBUS_H
#define _PHONEFSOD_DBUS_H

#include <gio/gio.h>

int phonefsod_dbus_setup();
void phonefsod_dbus_shutdown();

/* phoneuid - dbus callbacks */
void phoneui_show_incoming_cb(GObject *source, GAsyncResult *res, gpointer data);
void phoneui_hide_incoming_cb(GObject *source, GAsyncResult *res, gpointer data);
void phoneui_show_outgoing_cb(GObject *source, GAsyncResult *res, gpointer data);
void phoneui_hide_outgoing_cb(GObject *source, GAsyncResult *res, gpointer data);

void phoneui_display_message_cb(GObject *source, GAsyncResult *res, gpointer data);

void phoneui_show_dialog_cb(GObject *source, GAsyncResult *res, gpointer data);
void phoneui_show_sim_auth_cb(GObject *source, GAsyncResult *res, gpointer data);
void phoneui_hide_sim_auth_cb(GObject *source, GAsyncResult *res, gpointer data);
void phoneui_show_ussd_cb(GObject *source, GAsyncResult *res, gpointer data);

void phoneui_show_idle_cb(GObject *source, GAsyncResult *res, gpointer data);
void phoneui_hide_idle_cb(GObject *source, GAsyncResult *res, gpointer data);
void phoneui_toggle_idle_cb(GObject *source, GAsyncResult *res, gpointer data);
void phoneui_activate_screensaver_cb(GObject *source, GAsyncResult *res, gpointer data);
void phoneui_deactivate_screensaver_cb(GObject *source, GAsyncResult *res, gpointer data);

void phoneui_show_quick_settings_cb(GObject *source, GAsyncResult *res, gpointer data);

#endif
