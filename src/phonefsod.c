/*
 *  Copyright (C) 2008
 *      Authors (alphabetical) :
 *              Marc-Olivier Barre <marco@marcochapeau.org>
 *              Julien Cassignol <ainulindale@gmail.com>
 *              Andreas Engelbredt Dalsgaard <andreas.dalsgaard@gmail.com>
 *              Klaus 'mrmoku' Kurzmann <mok@fluxnetz.de>
 *              quickdev
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Public License as published by
 *  the Free Software Foundation; version 2 of the license.
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
#include <syslog.h>     /* openlog(), syslog(), closelog() */
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/inotify.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <glib-object.h>
#include <glib/gthread.h>

#include <frameworkd-glib/frameworkd-glib-dbus.h>

#include "phonefsod-dbus.h"
#include "phonefsod-fso.h"
#include "phonefsod-globals.h"


/* Program Standards passed from compiler */
#ifndef PACKAGE_VERSION
    #define PACKAGE_VERSION "0.1.0"
#endif
#ifndef PACKAGE_NAME
    #define PACKAGE_NAME "gdaemons_glib"
#endif
#ifndef PACKAGE_PIDFILE
    #define PACKAGE_PIDFILE "/var/run/"PACKAGE_NAME".pid"
#endif

#ifndef EXIT_ERROR
    #define EXIT_ERROR -1
#endif

// FIXME: ugly !!!
#define LOGFILE "/var/log/phonefsod.log"
#define MINIMUM_GSM_REREGISTER_TIMEOUT 60

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

/* handle for the logfile */
static int logfile = -1;

/* handle for notification on config changes */
static int notify;

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
	struct timeval tv;
	struct tm ptime;
	gettimeofday(&tv, NULL);
        localtime_r(&tv.tv_sec, &ptime);

	char *msg = g_strdup_printf("%04d.%02d.%02d %02d:%02d:%02d.%06d %s\n",
			ptime.tm_year+1900, ptime.tm_mon, ptime.tm_mday,
			ptime.tm_hour, ptime.tm_min, ptime.tm_sec, tv.tv_usec,
			message);
	write(logfile, msg, strlen(msg));
	free (msg);
}

static void
_load_config()
{
	GKeyFile *keyfile;
	GKeyFileFlags flags;
	GError *error = NULL;

	/* Read the phonefsod preferences */
	keyfile = g_key_file_new();
	flags = G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS;
	if (g_key_file_load_from_file
	    (keyfile, PHONEFSOD_CONFIG, flags, &error)) {
		show_incoming_sms =
			g_key_file_get_boolean(keyfile, "phonegui",
				       "show_incoming_sms", NULL);
		gsm_reregister_timeout =
			g_key_file_get_integer(keyfile, "phonefsod",
					"reregister_timeout", NULL);
		/* ensure a sane value for the timeout... minimum is 60s */
		if (gsm_reregister_timeout < MINIMUM_GSM_REREGISTER_TIMEOUT) {
			g_message("invalid reregister_timeout - setting to %ds",
					MINIMUM_GSM_REREGISTER_TIMEOUT);
			gsm_reregister_timeout = MINIMUM_GSM_REREGISTER_TIMEOUT;
		}
		g_debug("Configuration file read");
	}
	else {
		g_error(error->message);
		g_debug("Reading configuration file error, skipping");
	}
}

static void
_reload_config()
{
	g_debug("reloading configuration");
	_load_config();
}



 /* daemon_glib.c 
   
  James Scott Jr. <skoona@verizon.net>
  Linux Example Daemons - GLIB version
  GPL2 Copyright (C) 2008 James Scott, Jr. <skoona@users.sourceforge.net>
  date: 6/10/2008 

  syntax: gdaemon_glib [-h|--help] [-d|--debug [0|1]] [-u|--userid name ] [-v|--version] [-f|--forcepid]
                 
  debug=0 is off [default], 1=on, 88=on_console_mode no daemon fork
  force=1 create or overwrite the pidfile, =0 fail if exist
  
  gcc `pkg-config --libs --cflags glib-2.0 gthread-2.0 gobject-2.0` -Wall -o gdaemons daemon_glib.c
  
  Files:
  1. /var/run/gdaemons_glib.pid
  2. /etc/init.d/gdaemons
  
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/  


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
	default:
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
	*context = g_option_context_new (" => GLIB Daemon Example [2008] <skoona@users.sourceforge.net>");
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
			"GPLv2 (2009) <mok@fluxnetz.de>");
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

	/* Create the PID File to show we are active */
	if (gd_b_force) {
		pidfile = g_open(pidfilename, O_WRONLY|O_CREAT|O_TRUNC,
				S_IRUSR |S_IWUSR|S_IRGRP|S_IROTH);
	}
	else {
		pidfile = g_open(pidfilename, O_WRONLY|O_CREAT|O_EXCL,
				S_IRUSR |S_IWUSR|S_IRGRP|S_IROTH);
	}
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

	/* initialize logging */
	logfile = open(LOGFILE, O_WRONLY|O_CREAT|O_APPEND);
	if (logfile == -1) {
		printf("failed creating the logfile (%s)", LOGFILE);
		return (-3);
	}
	g_log_set_default_handler(_log_handler, NULL);

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

	_load_config();

	/* setup dbus server part */
	phonefsod_dbus_setup();

	/* initialize libframeworkd-glib */
	FrameworkdHandler *fwHandler = frameworkd_handler_new();
	fwHandler->simAuthStatus = fso_sim_auth_status_handler;
	fwHandler->simReadyStatus = fso_sim_ready_status_handler;
	//fwHandler->simIncomingStoredMessage =
	//	fso_sim_incoming_stored_message_handler;
	fwHandler->callCallStatus = fso_call_status_handler;
	fwHandler->deviceIdleNotifierState =
		fso_device_idle_notifier_state_handler;
	fwHandler->incomingUssd = fso_incoming_ussd_handler;

	fwHandler->usageResourceAvailable =
		fso_resource_available_handler;
	fwHandler->usageResourceChanged = fso_resource_changed_handler;
	fwHandler->networkStatus = fso_network_status_handler;
	fwHandler->pimIncomingMessage = fso_incoming_message_handler;

	frameworkd_handler_connect(fwHandler);

	g_debug("connected to FSO");

	//notify = inotify_init();
	//inotify_add_watch(notify, PHONEFSOD_CONFIG, IN_MODIFY);

	/* Start glib main loop and run list_resources() */
	g_debug("entering glib main loop");
	g_timeout_add(0, fso_list_resources, NULL);
	g_main_loop_run(main_loop);

	/* Cleanup and exit */
	trc = g_thread_join(sig_thread);
	g_message("Signal thread was ended by a %s signal.",
			g_strsignal(GPOINTER_TO_INT(trc)) );

	pthread_sigmask (SIG_UNBLOCK, &signal_set, NULL);
	g_option_context_free (context);
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


