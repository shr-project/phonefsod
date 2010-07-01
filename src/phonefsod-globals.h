
#ifndef _PHONEFSOD_GLOBALS_H
#define _PHONEFSOD_GLOBALS_H

gboolean offline_mode;
gboolean sim_auth_needed;
int inhibit_suspend_on_startup_time;
gboolean show_incoming_sms;
int gsm_reregister_timeout;
char *pdp_apn;
char *pdp_user;
char *pdp_password;

int default_brightness;
int minimum_brightness;
int dim_idle_percent;
int dim_idle_dim_percent;
int dim_idle_prelock_percent;


enum DimScreen {
	DIM_SCREEN_NEVER,
	DIM_SCREEN_ONBAT,
	DIM_SCREEN_ALWAYS
} dim_screen;

enum IdleScreen {
	IDLE_SCREEN_NEVER = 0,
	IDLE_SCREEN_AUX = 1,
	IDLE_SCREEN_LOCK = 2,
	IDLE_SCREEN_PHONE = 4,
	IDLE_SCREEN_SUSPEND = 8
} idle_screen;

enum AutoSuspend {
	SUSPEND_NEVER = 0,
	SUSPEND_NORMAL,
	SUSPEND_ALWAYS
} auto_suspend;


DBusGConnection *system_bus;

#endif
