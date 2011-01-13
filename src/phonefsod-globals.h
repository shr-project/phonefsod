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


#ifndef _PHONEFSOD_GLOBALS_H
#define _PHONEFSOD_GLOBALS_H

#include <gio/gio.h>
#include <freesmartphone.h>
#include <shr-bindings.h>

gboolean offline_mode;
gboolean sim_auth_needed;
int inhibit_suspend_on_startup_time;
gboolean show_incoming_sms;
int gsm_reregister_timeout;
FreeSmartphoneGSMCallingIdentificationStatus calling_identification;
char *pdp_apn;
char *pdp_user;
char *pdp_password;
char *sim_pin;

int default_brightness;
int minimum_brightness;
int dim_idle_percent;
int dim_idle_dim_percent;
int dim_idle_prelock_percent;


enum DimScreenMode {
	DIM_SCREEN_NEVER,
	DIM_SCREEN_ONBAT,
	DIM_SCREEN_ALWAYS
} dim_screen;

enum IdleScreenMode {
	IDLE_SCREEN_NEVER = 0,
	IDLE_SCREEN_AUX = 1,
	IDLE_SCREEN_LOCK = 2,
	IDLE_SCREEN_PHONE = 4,
	IDLE_SCREEN_SUSPEND = 8
} idle_screen;

enum AutoSuspendMode {
	SUSPEND_NEVER = 0,
	SUSPEND_NORMAL,
	SUSPEND_ALWAYS
} auto_suspend;


GDBusConnection *system_bus;

struct SPhoneui {
	PhoneuiCallManagement *call_management;
	PhoneuiMessages *messages;
	PhoneuiNotification *notification;
	PhoneuiIdleScreen *idle_screen;
} phoneui;


#endif
