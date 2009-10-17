#ifndef _PHONEFSOD_FSO_H
#define _PHONEFSOD_FSO_H



/* GSM resource handling */
gboolean
fso_list_resources();
gboolean
fso_request_gsm();
gboolean
fso_set_antenna_power();
gboolean
fso_register_network();
void
fso_sim_ready_actions(void);

/* signal handlers */
void
fso_resource_available_handler(const char *name,
		gboolean availability);
void
fso_resource_changed_handler(const char *name, gboolean state,
				    GHashTable * attributes);
void
fso_device_idle_notifier_power_state_handler(GError * error,
						    const int status,
						    gpointer userdata);
void
fso_device_idle_notifier_state_handler(const int state);
void
fso_call_status_handler(const int call_id, const int status,
			       GHashTable * properties);
void
fso_sim_auth_status_handler(const int status);
void
fso_sim_ready_status_handler(gboolean status);
void
fso_incoming_message_handler(char *message_path);
void
fso_sim_incoming_stored_message_handler(const int id);
void
fso_incoming_ussd_handler(int mode, const char *message);
void
fso_network_status_handler(GHashTable *status);

#endif

