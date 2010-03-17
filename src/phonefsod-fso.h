#ifndef _PHONEFSOD_FSO_H
#define _PHONEFSOD_FSO_H

#include <glib.h>

gboolean fso_init();
gboolean fso_startup();
void fso_dimit(int percent);
void fso_get_resource_state(const char *resource, void (*callback)(GError *, gboolean, gpointer), gpointer data);
gboolean fso_set_functionality();

#endif
