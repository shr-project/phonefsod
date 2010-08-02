/*
 *  Copyright (C) 2009-2010
 *      Authors (alphabetical) :
 *              Klaus 'mrmoku' Kurzmann <mok@fluxnetz.de>
 *
 * glib daemon framework
 *              James Scott Jr. <skoona@verizon.net>
 *              Copyright (C) 2008 James Scott, Jr. <skoona@users.sourceforge.net>
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

#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>        /* umask() */
#include <sys/stat.h>   /* umask() */
#include <fcntl.h>      /* open() */
#include <errno.h>
#include <unistd.h>     /* daemon(), sleep(), exit() */
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/inotify.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <glib-object.h>
#include <glib/gthread.h>
#include <dbus/dbus-glib.h>

#include <fsoframework.h>
#include <freesmartphone.h>

#include "phonefsod-dbus.h"
#include "phonefsod-dbus-phoneuid.h"
#include "phonefsod-fso.h"
#include "phonefsod-globals.h"


/* Program Standards passed from compiler */
#ifndef PACKAGE_VERSION
    #define PACKAGE_VERSION "0.1.0"
#endif
#ifndef PACKAGE_NAME
    #define PACKAGE_NAME "phonefsod"
#endif
#ifndef PACKAGE_PIDFILE
    #define PACKAGE_PIDFILE "/var/run/"PACKAGE_NAME".pid"
#endif

#ifndef EXIT_ERROR
    #define EXIT_ERROR -1
#endif

// FIXME: ugly !!!
#define LOGFILE "/var/log/phonefsod.log"
#define DEFAULT_DEBUG_LEVEL "INFO"

/* defines for config defaults */
#define MINIMUM_GSM_REREGISTER_TIMEOUT 60
#define DEFAULT_GSM_REREGISTER_TIMEOUT 200
#define DEFAULT_DEFAULT_BRIGHTNESS 100
#define DEFAULT_MINIMUM_BRIGHTNESS 10

/* global variable used to indicate that
 * program exit is desired 1=run, 0=exit */
static volatile gint gd_flag_exit = 1;

/* Target user id */
static gchar *gd_pch_effective_userid = NULL;

/* PID Filename */
static gchar *gd_pch_pid_filename = NULL;

/* Debug flag */
static gint i_debug = 0;

/* Force overwrite of pidfile */
static gboolean gd_b_force = FALSE;

/* Version flag */
static gboolean gd_b_version = FALSE;

/* file stream for the logfile */
static FILE *logfile = NULL;

/* handle for notification on config changes */
static int notify;

static GLogLevelFlags log_flags;

/* Local Routines */
static gpointer _thread_handle_signals(gpointer arg);
static gint _process_signals(siginfo_t *signal_info);
static gint _handle_command_line(int argc, char **argv, GOptionContext **context);
static gint _daemonize(gchar *pidfilename);
static void _log_handler(const gchar *domain, GLogLevelFlags level,
		const gchar *message, gpointer userdata);

extern int main (int argc, char *argv[]);

static void
_log_handler(const gchar *domain, GLogLevelFlags level, const gchar *message,
		gpointer userdata)
{
	char *levelstr;
	char date_str[30];
	struct timeval tv;
	struct tm ptime;
	if (!(log_flags & G_LOG_LEVEL_MASK & level)) {
		return;
	}
	gettimeofday(&tv, NULL);
	localtime_r(&tv.tv_sec, &ptime);

	strftime(date_str, 30, "%Y.%m.%d %T", &ptime);

	switch (level) {
	case G_LOG_LEVEL_ERROR:
		levelstr = "ERROR";
		break;
	case G_LOG_LEVEL_CRITICAL:
		levelstr = "CRITICAL";
		break;
	case G_LOG_LEVEL_WARNING:
		levelstr = "WARNING";
		break;
	case G_LOG_LEVEL_MESSAGE:
		levelstr = "MESSAGE";
		break;
	case G_LOG_LEVEL_INFO:
		levelstr = "INFO";
		break;
	case G_LOG_LEVEL_DEBUG:
		levelstr = "DEBUG";
		break;
	default:
		levelstr = "";
		break;
	}

	fprintf(logfile, "%s.%06d [%s]\t%s: %s\n", date_str, (int) tv.tv_usec,
			domain, levelstr, message);
	fflush(logfile);
}

static void
_load_config()
{
	GKeyFile *keyfile;
	GKeyFileFlags flags;
	GError *error = NULL;
	char *debug_level = NULL;
	char *logpath = NULL;
	char *s = NULL;

	/* Read the phonefsod preferences */
	keyfile = g_key_file_new();
	flags = G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS;
	if (g_key_file_load_from_file
	    (keyfile, PHONEFSOD_CONFIG, flags, &error)) {

		/* --- [logging] --- */
		logpath = g_key_file_get_string(keyfile, "logging",
					"log_file", NULL);
		debug_level =
			g_key_file_get_string(keyfile, "logging",
					"log_level", NULL);

		/* --- [gsm] --- */
		offline_mode =
			g_key_file_get_boolean(keyfile, "gsm",
					"offline_mode", &error);
		if (error) {
			offline_mode = FALSE;
			g_error_free(error);
			error = NULL;
		}

		inhibit_suspend_on_startup_time =
			g_key_file_get_integer(keyfile, "gsm",
					       "inhibit_suspend_on_startup_time",
					       &error);
		if (error) {
			inhibit_suspend_on_startup_time = 360;
			g_error_free(error);
			error = NULL;
		}

		show_incoming_sms =
			g_key_file_get_boolean(keyfile, "gsm",
				       "show_incoming_sms", &error);
		if (error) {
			show_incoming_sms = TRUE;
			g_error_free(error);
			error = NULL;
		}
		gsm_reregister_timeout =
			g_key_file_get_integer(keyfile, "gsm",
					"reregister_timeout", &error);
		if (error) {
			gsm_reregister_timeout = DEFAULT_GSM_REREGISTER_TIMEOUT;
			g_error_free(error);
			error = NULL;
		}
		/* ensure a sane value for the timeout... minimum is 60s */
		else if (gsm_reregister_timeout < MINIMUM_GSM_REREGISTER_TIMEOUT) {
			g_message("invalid reregister_timeout - setting to %ds",
					MINIMUM_GSM_REREGISTER_TIMEOUT);
			gsm_reregister_timeout = MINIMUM_GSM_REREGISTER_TIMEOUT;
		}

		s = g_key_file_get_string(keyfile, "gsm",
					  "calling_identification", &error);
		if (error) {
			calling_identification = FREE_SMARTPHONE_GSM_CALLING_IDENTIFICATION_STATUS_NETWORK;
			g_error_free(error);
			error = NULL;
		}
		else {
			if (!strncmp("off", s, 3)) {
				calling_identification = FREE_SMARTPHONE_GSM_CALLING_IDENTIFICATION_STATUS_OFF;
			}
			else if (!strncmp("on", s, 2)) {
				calling_identification = FREE_SMARTPHONE_GSM_CALLING_IDENTIFICATION_STATUS_ON;
			}
			else if (!strncmp("network", s, 7)) {
				calling_identification = FREE_SMARTPHONE_GSM_CALLING_IDENTIFICATION_STATUS_NETWORK;
			}
			else {
				g_warning("Invalid value '%s' for calling_identification in [gsm] section of %s",
					  s, PHONEFSOD_CONFIG);
				g_message("Defaulting to network");
				calling_identification = FREE_SMARTPHONE_GSM_CALLING_IDENTIFICATION_STATUS_NETWORK;
			}
		}

		pdp_apn = g_key_file_get_string(keyfile, "gsm",
						"pdp_apn", &error);
		if (error) {
			g_error_free(error);
			error = NULL;
		}
		pdp_user = g_key_file_get_string(keyfile, "gsm",
						 "pdp_user", &error);
		if (error) {
			g_error_free(error);
			error = NULL;
		}
		pdp_password = g_key_file_get_string(keyfile, "gsm",
						     "pdp_password", &error);
		if (error) {
			g_error_free(error);
			error = NULL;
		}

		sim_pin = g_key_file_get_string(keyfile, "gsm", "pin", &error);
		if (error) {
			g_error_free(error);
			error = NULL;
		}

		/* --- [idle] --- */
		default_brightness =
			g_key_file_get_integer(keyfile, "idle",
					"default_brightness", &error);
		if (error) {
			default_brightness = DEFAULT_DEFAULT_BRIGHTNESS;
			g_error_free(error);
			error = NULL;
		}
		s = g_key_file_get_string(keyfile, "idle",
						"dim_screen", &error);
		if (error) {
			dim_screen = DIM_SCREEN_ALWAYS;
			g_error_free(error);
			error = NULL;
		}
		else {
			if (!strncmp("never", s, 5)) {
				dim_screen = DIM_SCREEN_NEVER;
			}
			else if (!strncmp("onbat", s, 5)) {
				dim_screen = DIM_SCREEN_ONBAT;
			}
			else if (!strncmp("always", s, 6)) {
				dim_screen = DIM_SCREEN_ALWAYS;
			}
			else {
				g_warning("Invalid value '%s' for dim_screen in [idle] section of %s",
					  s, PHONEFSOD_CONFIG);
				g_message("Defaulting to DIM_SCREEN_ALWAYS");
				dim_screen = DIM_SCREEN_ALWAYS;
			}
		}
		minimum_brightness =
			g_key_file_get_integer(keyfile, "idle",
					"minimum_brightness", &error);
		if (error) {
			minimum_brightness = DEFAULT_MINIMUM_BRIGHTNESS;
			g_error_free(error);
			error = NULL;
		}
		dim_idle_percent =
			g_key_file_get_integer(keyfile, "idle",
					"dim_idle_percent", &error);
		if (error) {
			dim_idle_percent = -1;
			g_error_free(error);
			error = NULL;
		}
		dim_idle_dim_percent =
			g_key_file_get_integer(keyfile, "idle",
					"dim_idle_dim_percent", &error);
		if (error) {
			dim_idle_dim_percent = -1;
			g_error_free(error);
			error = NULL;
		}
		dim_idle_prelock_percent =
			g_key_file_get_integer(keyfile, "idle",
					"dim_idle_prelock_percent", &error);
		if (error) {
			dim_idle_prelock_percent = -1;
			g_error_free(error);
			error = NULL;
		}
		s = g_key_file_get_string(keyfile, "idle",
					"idle_screen", &error);
		if (error) {
			g_debug("no idle_screen found in config - defaulting to lock,aux");
			idle_screen = IDLE_SCREEN_LOCK | IDLE_SCREEN_AUX | IDLE_SCREEN_SUSPEND;
			g_error_free(error);
			error = NULL;
		}
		else {
			int i;
			gchar **flags = g_strsplit(s, ",", 0);
			idle_screen = IDLE_SCREEN_NEVER;
			for (i = 0; flags[i]; i++) {
				if (strcmp(flags[i], "lock") == 0) {
					g_debug("adding LOCK to idle_screen");
					idle_screen |= IDLE_SCREEN_LOCK;
				}
				else if (strcmp(flags[i], "aux") == 0) {
					g_debug("adding AUX to idle_scren");
					idle_screen |= IDLE_SCREEN_AUX;
				}
				else if (strcmp(flags[i], "phone") == 0) {
					g_debug("adding PHONE to idle_screen");
					idle_screen |= IDLE_SCREEN_PHONE;
				}
				else if (strcmp(flags[i], "suspend") == 0) {
					g_debug("adding SUSPEND to idle_screen");
					idle_screen |= IDLE_SCREEN_SUSPEND;
				}
			}
			g_strfreev(flags);
			free(s);
		}
		s = g_key_file_get_string(keyfile, "idle",
				"auto_suspend", &error);
		if (error) {
			auto_suspend = SUSPEND_NORMAL;
			g_error_free(error);
			error = NULL;
		}
		else if (strcmp(s, "never") == 0) {
			auto_suspend = SUSPEND_NEVER;
		}
		else if (strcmp(s, "always") == 0) {
			auto_suspend = SUSPEND_ALWAYS;
		}
		else {
			auto_suspend = SUSPEND_NORMAL;
		}
		if (s)
			free(s);

		g_debug("Configuration file read");
	}
	else {
		g_warning(error->message);
		g_error_free(error);
	}

	debug_level = (debug_level) ? debug_level : DEFAULT_DEBUG_LEVEL;
	logpath = (logpath) ? logpath : LOGFILE;

	log_flags = G_LOG_FLAG_FATAL;
	if (!strcmp(debug_level, "DEBUG")) {
		log_flags |= G_LOG_LEVEL_MASK;
	}
	else if (!strcmp(debug_level, "INFO")) {
		log_flags |= G_LOG_LEVEL_MASK ^ (G_LOG_LEVEL_DEBUG);
	}
	else if (!strcmp(debug_level, "MESSAGE")) {
		log_flags |= G_LOG_LEVEL_MASK ^ (G_LOG_LEVEL_DEBUG
			| G_LOG_LEVEL_INFO);
	}
	else if (!strcmp(debug_level, "WARNING")) {
		log_flags |= G_LOG_LEVEL_MASK ^ (G_LOG_LEVEL_DEBUG
			| G_LOG_LEVEL_INFO | G_LOG_LEVEL_MESSAGE);
	}
	else if (!strcmp(debug_level, "CRITICAL")) {
		log_flags |= G_LOG_LEVEL_MASK ^ (G_LOG_LEVEL_DEBUG
			| G_LOG_LEVEL_INFO | G_LOG_LEVEL_MESSAGE
			| G_LOG_LEVEL_WARNING);
	}
	else if (!strcmp(debug_level, "ERROR")) {
		log_flags |= G_LOG_LEVEL_MASK ^ (G_LOG_LEVEL_DEBUG
			| G_LOG_LEVEL_INFO | G_LOG_LEVEL_MESSAGE
			| G_LOG_LEVEL_WARNING | G_LOG_LEVEL_CRITICAL);
	}
	else {
	}

	/* initialize logging */
	logfile = fopen(logpath, "a");
	if (!logfile) {
		printf("Error creating the logfile (%s) !!!", logpath);
	}
	else {
		g_log_set_default_handler(_log_handler, NULL);
	}
}

static void
_reload_config()
{
	g_debug("reloading configuration");
	_load_config();
}



/*
 * _process_signals()
 *
 * Handle/Process linux signals for the whole multi-threaded application.
 *
 * Params:
 *    sig -- current linux signal
 *
 * Returns/Affects:
 *   returns current value of the atomic int gd_flag_exit
 *   returns true (or current value) if nothing needs done
 *   returns 0 or false if exit is required
*/
static gint _process_signals ( siginfo_t *signal_info)
{
	int rval = g_atomic_int_get(&gd_flag_exit); /* use existing value */
	gint sig = 0;
	gchar *pch = "<unknown>";

	g_return_val_if_fail (signal_info != NULL, 0);
	sig = signal_info->si_signo;

	/* look to see what signal has been caught */
	switch ( sig ) {
	case SIGHUP:    /* often used to reload configuration */
        case SIGUSR1:   /* Any user function */
		switch (signal_info->si_code) {
		case SI_USER:  pch="kill(2) or raise(3)"; break;
		case SI_KERNEL:  pch="Sent by the kernel."; break;
		case SI_QUEUE:  pch="sigqueue(2)"; break;
		case SI_TIMER:  pch="POSIX timer expired"; break;
		case SI_MESGQ:  pch="POSIX message queue state changed"; break;
		case SI_ASYNCIO:  pch="AIO completed"; break;
		case SI_SIGIO:  pch="queued SIGIO"; break;
		case SI_TKILL:  pch="tkill(2) or tgkill(2)"; break;
		default: pch = "<unknown>"; break;
		}
		g_debug("%s received from => %s ?[pid=%d, uid=%d]{Ignored}",
                  g_strsignal(sig), pch, signal_info->si_pid,signal_info->si_uid);
		break;
	case SIGCHLD:   /* some child ended */
		switch (signal_info->si_code) {
		case CLD_EXITED: pch = "child has exited"; break;
		case CLD_KILLED: pch = "child was killed"; break;
		case CLD_DUMPED: pch = "child terminated abnormally"; break;
		case CLD_TRAPPED: pch = "traced child has trapped"; break;
		case CLD_STOPPED: pch = "child has stopped"; break;
		case CLD_CONTINUED: pch = "stopped child has continued"; break;
		default: pch = "<unknown>"; break;
		}
		g_debug("%s received for pid => %d, w/rc => %d for this reason => %s {Ignored}",
                    g_strsignal(sig), signal_info->si_pid, signal_info->si_status, pch);
		break;
	case SIGQUIT:   /* often used to signal an orderly shutdown */
	case SIGINT:    /* often used to signal an orderly shutdown */
	case SIGPWR:    /* Power Failure */
	case SIGKILL:   /* Fatal Exit flag */
	case SIGTERM:   /* Immediately Fatal Exit flag */
		switch (signal_info->si_code) {
		case SI_USER:  pch="kill(2) or raise(3)"; break;
		case SI_KERNEL:  pch="Sent by the kernel."; break;
		case SI_QUEUE:  pch="sigqueue(2)"; break;
		case SI_TIMER:  pch="POSIX timer expired"; break;
		case SI_MESGQ:  pch="POSIX message queue state changed"; break;
		case SI_ASYNCIO:  pch="AIO completed"; break;
		case SI_SIGIO:  pch="queued SIGIO"; break;
		case SI_TKILL:  pch="tkill(2) or tgkill(2)"; break;
		default: pch = "<unknown>"; break;
		}
		g_debug("%s received from => %s ?[pid=%d, uid=%d]{Exiting}",
			g_strsignal(sig), pch, signal_info->si_pid,
			signal_info->si_uid);
		rval = 0;
		break;
	default:
		g_debug("%s received => {Ignored}", g_strsignal(sig));
		break;
	} /* end switch */

	return (rval);
}

/*
 *  _thread_handle_signals()
 *
 *  Trap linux signals for the whole multi-threaded application.
 *
 *  Params:
 *    main_loop  -- closing this shuts down the app orderly
 *
 *  Returns/Affects:
 *      returns and/or set the atomic gint gd_flag_exit
 *      returns last signal
*/
static gpointer  _thread_handle_signals(gpointer main_loop)
{
	sigset_t signal_set;
	siginfo_t signal_info;
	gint    sig = 0;
	gint   rval = 0;

	sigfillset (&signal_set);
	g_debug("signal handler: startup successful");

	while (g_atomic_int_get(&gd_flag_exit)) {
		/* wait for any and all signals */
		sig = sigwaitinfo (&signal_set, &signal_info);
		if (!sig) {
			g_message("signal handler: sigwaitinfo() returned an error => {%s}",
				g_strerror(errno));
			continue;
		}
		/* when we get this far, we've  caught a signal */
		rval = _process_signals ( &signal_info );
		g_atomic_int_set(&gd_flag_exit, rval);

	} /* end-while */

	g_main_loop_quit(main_loop);

	pthread_sigmask (SIG_UNBLOCK, &signal_set, NULL);

	g_debug("signal handler: shutdown complete");

	g_thread_exit ( GINT_TO_POINTER(sig) );

	return (NULL);
}

/*
 *  _handle_command_line()
 *
 *  Parse out the command line options from argc,argv
 *  gdaemon_glib [-u|--userid name] [-f|--forcepid] [-p|--pidfile fname]
 *               [-d|--debug XX]  [-v|--version] [-h|--help]
 *
 *  Returns: TRUE if all params where handled
 *          FALSE is any error occurs or an option required shutdown
*/
static gint _handle_command_line(int argc, char **argv,
		GOptionContext **context)
{
	GError *gerror = NULL;
	GOptionEntry entries[] = {
		{"userid", 'u', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING,
			&gd_pch_effective_userid, "Runtime userid", "name"},
		{"pidfile", 'p', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING,
			&gd_pch_pid_filename, "PID Filename", PACKAGE_PIDFILE},
		{"debug", 'd', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_INT,
			&i_debug, "Turn on debug messages", "[0|1]"},
		{"forcepid", 'f', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&gd_b_force, "Force overwite of pid file",
			"cleanup after prior errors"},
		{"version", 'v', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
			&gd_b_version, "Program version info", NULL},
		{NULL}
	};

	/* Get command line parms */
	*context = g_option_context_new (" => SHR Phone FSO Daemon");
	g_option_context_add_main_entries(*context, entries, NULL);
	g_option_context_set_ignore_unknown_options(*context, FALSE);
	if (!(g_option_context_parse(*context, &argc, &argv, &gerror))) {
		g_warning ("Parse command line failed: %s", gerror->message);
		g_option_context_free(*context);
		g_error_free(gerror);
		g_atomic_int_set(&gd_flag_exit, 0); /* flag an exit */

		return (EXIT_FAILURE);
	}

	if (gd_b_version) {
		g_print ("SHR PhoneFSO Daemon\n%s Version %s\n%s\n\n",
			PACKAGE_NAME, PACKAGE_VERSION,
			"GPLv2 (2009) the SHR Team");
		g_option_context_free(*context);
		g_atomic_int_set(&gd_flag_exit, 0); /* flag an exit */

		return (EXIT_FAILURE);
	}

	if (gd_pch_pid_filename == NULL) {
		gd_pch_pid_filename = PACKAGE_PIDFILE;
	}

	return (EXIT_SUCCESS);
}

/*
 * _daemonize()
 *
 * The single fork method of becoming a daemon, causes the process
 * to become a session leader and process group leader.
 *
 * Params:
 *      gd_pch_pid_filename -- pid filename
 *      gd_b_force          -- global to control overwrite of pid file
 *
 *  Returns:
 *      returns integer which is passed back to the parent process
 *      values - EXIT_FAILURE Parent and 1st Child
 *             - EXIT_ERROR   AnyPid or error
 *             - EXIT_SUCCESS Only the Daemon or GrandChild
*/
static gint _daemonize (gchar *pidfilename)
{
	gint pidfile = 0;
	pid_t pid = 0;
	pid_t sid = 0;
	gint len = 0;
	gchar ch_buff[16];

	g_return_val_if_fail(pidfilename != NULL, EXIT_ERROR);

	/* Fork off the parent process */
	switch (pid = fork()) {
	case -1: /* error -- all out */
		g_warning("Shutting down as Pid[%d]: fork(error=%s)",
				getpid(), strerror(errno));
		return (EXIT_ERROR);
		break;
	case 0: /* new process */
		sleep(1);
		break;
	default: /* Normal exit, pid equals child real pid */
		return (EXIT_FAILURE);
	}

	/* Change the file mode mask */
	umask(0);

	/* Change the current working directory */
	if ((g_chdir("/")) < 0) {
		g_warning("Child[%d] is exiting: chdir(error=%s)",
				getpid(), strerror(errno));
		exit(EXIT_ERROR);
	}

	/* Close out the standard file descriptors */
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	/* Create a new SID for the child process */
	sid = setsid();
	if (sid < 0) {
		g_warning("Child[%d] is exiting: setsid(error=%s)",
				getpid(), strerror(errno));
		exit(EXIT_ERROR);
	}

	pidfile = g_open(pidfilename, O_WRONLY|O_CREAT|O_TRUNC,
			S_IRUSR |S_IWUSR|S_IRGRP|S_IROTH);
	if (pidfile == -1) {
		g_warning("Child Error: cannot open pidfile %s: %s",
				pidfilename, strerror(errno));
		return (EXIT_ERROR);
	}

	len = snprintf(ch_buff, sizeof(ch_buff), "%d", getpid());
	write(pidfile, ch_buff, len);
	if (close(pidfile) != 0) {
		g_error("Child Warning: cannot close pidfile %s: %s",
				pidfilename, strerror(errno));
	}

	return (EXIT_SUCCESS);
}

static void
_name_owner_changed(DBusGProxy *proxy, const char *name,
		    const char *prev, const char *new, gpointer data)
{
	(void) proxy;
	(void) data;
	g_debug("NameOwnerChanged: %s / %s / %s", name, prev, new);
	if (new && *new) {
		if (sim_auth_needed && !strcmp(name, "org.shr.phoneui")) {
			phoneuid_notification_show_sim_auth(0);
			sim_auth_needed = FALSE;
			return;
		}
		if (!strcmp(name, FSO_FRAMEWORK_GSM_ServiceDBusName)) {
			fso_startup();
		}
/*		if (!strcmp(name, FSO_FRAMEWORK_USAGE_ServiceDBusName)) {
			fso_connect_usage();
		}
		else if (!strcmp(name, FSO_FRAMEWORK_GSM_ServiceDBusName)) {
			fso_connect_gsm();
		}
		else if (!strcmp(name, FSO_FRAMEWORK_PIM_ServiceDBusName)) {
			fso_connect_pim();
		}
		else if (!strcmp(name, FSO_FRAMEWORK_DEVICE_ServiceDBusName)) {
			fso_connect_device();
		}*/
	}
}

/* Main Entry Point for this Daemon */
extern int main (int argc, char *argv[])
{
	sigset_t   signal_set;
	GMainLoop *main_loop  = NULL;
	GError    *gerror     = NULL;
	GThread   *sig_thread = NULL;
	GOptionContext *context = NULL;
	gpointer   trc = NULL;

	uid_t     real_user_id = 0;
	uid_t     effective_user_id = 0;
	gint      rc = 0;
	struct    passwd *userinfo = NULL;
	DBusGProxy *dbus_proxy;

	/* initialize threading and mainloop */
	g_type_init();
	g_thread_init(NULL);
	main_loop = g_main_loop_new (NULL, FALSE);

	/* handle command line arguments */
	if ((rc = _handle_command_line( argc, argv, &context)) != EXIT_SUCCESS) {
		g_main_loop_unref (main_loop);
		exit (rc);
	}

	/* remember real and effective userid to restore them on shutdown */
	real_user_id = getuid ();
	if (gd_pch_effective_userid != NULL) {
		userinfo = getpwnam(gd_pch_effective_userid);
		effective_user_id = userinfo->pw_uid;
	}
	else {
		effective_user_id = geteuid();
		userinfo = getpwuid(effective_user_id);
		gd_pch_effective_userid = userinfo->pw_name;
	}

	g_message("%s-%s is in startup mode as user(%s)",
		PACKAGE_NAME, PACKAGE_VERSION, gd_pch_effective_userid);

	/* daemonize */
	if (!i_debug) {
		switch (_daemonize(gd_pch_pid_filename)) {
		case EXIT_SUCCESS:  /* grandchild */
			break;
		case EXIT_ERROR:    /* any error */
			exit (EXIT_FAILURE);
			break;
		default:            /* parent or child pids */
			exit (EXIT_SUCCESS);
			break;
		}
	}
	else {
		g_message("Skipping daemonizing process");
	}

	/* become the requested user */
	seteuid (effective_user_id);

	if (!i_debug) {
		/* block all signals */
		sigfillset (&signal_set);
		pthread_sigmask (SIG_BLOCK, &signal_set, NULL);

		/* create the signal handling thread */
		sig_thread = g_thread_create ((GThreadFunc)_thread_handle_signals,
				main_loop, TRUE, &gerror);
		if (gerror != NULL) {
			g_message("Create signal thread failed: %s", gerror->message);
			g_error_free(gerror);
			g_option_context_free(context);
			g_main_loop_unref (main_loop);
			exit (EXIT_FAILURE);
		}
	}

	_load_config();

	system_bus = dbus_g_bus_get(DBUS_BUS_SYSTEM, &gerror);
	if (gerror) {
		g_error("%d: %s", gerror->code, gerror->message);
		g_error_free(gerror);
		g_option_context_free(context);
		g_main_loop_unref(main_loop);
		exit(EXIT_FAILURE);
	}

	/* register for NameOwnerChanged */
	dbus_proxy = dbus_g_proxy_new_for_name (system_bus,
			DBUS_SERVICE_DBUS, DBUS_PATH_DBUS, DBUS_INTERFACE_DBUS);
	dbus_g_proxy_add_signal(dbus_proxy, "NameOwnerChanged", G_TYPE_STRING,
				G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);

	dbus_g_proxy_connect_signal(dbus_proxy, "NameOwnerChanged",
				    G_CALLBACK(_name_owner_changed), NULL, NULL);


	/* connect and init FSO */
	if (!fso_init()) {
		g_option_context_free(context);
		g_main_loop_unref(main_loop);
		exit(EXIT_FAILURE);
	}

	phonefsod_dbus_setup();

	//notify = inotify_init();
	//inotify_add_watch(notify, PHONEFSOD_CONFIG, IN_MODIFY);

	/* Start glib main loop and run list_resources() */
	g_debug("entering glib main loop");
	g_timeout_add(0, fso_startup, NULL);
	g_main_loop_run(main_loop);

	/* Cleanup and exit */
	if (!i_debug) {
		trc = g_thread_join(sig_thread);
		g_message("Signal thread was ended by a %s signal.",
				g_strsignal(GPOINTER_TO_INT(trc)) );

		pthread_sigmask (SIG_UNBLOCK, &signal_set, NULL);
		g_option_context_free (context);
	}
	g_main_loop_unref (main_loop);

//	if (incoming_calls)
//		free(incoming_calls);
//	if (outgoing_calls)
//		free(outgoing_calls);

	/* become the privledged user again */
	seteuid (real_user_id);

	/* Remove the PID File to show we are inactive */
	if (!i_debug) {
		if (g_unlink(gd_pch_pid_filename) != 0) {
			g_warning("Main Error: cannot unlink/remove pidfile %s: %s",
					gd_pch_pid_filename, strerror(errno));
		}
	}

	/* write shutdown messages */
	g_message("%s-%s clean shutdown", PACKAGE_NAME, PACKAGE_VERSION);

	exit (EXIT_SUCCESS);
}


