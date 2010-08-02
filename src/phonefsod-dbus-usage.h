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

#ifndef _PHONEFSOD_DBUS_USAGE_H
#define _PHONEFSOD_DBUS_USAGE_H

#include <glib-object.h>

#define PHONEFSOD_TYPE_USAGE_SERVICE            (phonefsod_usage_service_get_type ())
#define PHONEFSOD_USAGE_SERVICE(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), PHONEFSOD_TYPE_USAGE_SERVICE, PhonefsodUsageService))
#define PHONEFSOD_USAGE_SERVICE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), PHONEFSOD_TYPE_USAGE_SERVICE, PhonefsodUsageServiceClass))
#define PHONEFSOD_IS_USAGE_SERVICE(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), PHONEFSOD_TYPE_USAGE_SERVICE))
#define PHONEFSOD_IS_USAGE_SERVICE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), PHONEFSOD_TYPE_USAGE_SERVICE))
#define PHONEFSOD_USAGE_SERVICE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), PHONEFSOD_TYPE_USAGE_SERVICE, PhonefsodUsageServiceClass))


typedef struct _PhonefsodUsageService PhonefsodUsageService;
typedef struct _PhonefsodUsageServiceClass PhonefsodUsageServiceClass;

GType phonefsod_usage_service_get_type(void);

struct _PhonefsodUsageService {
	GObject parent;
};

struct _PhonefsodUsageServiceClass {
	GObjectClass parent;
	DBusGConnection *connection;
};

void phonefsod_usage_service_set_offline_mode(PhonefsodUsageService *object,
		gboolean mode, DBusGMethodInvocation *context);

void phonefsod_usage_service_get_offline_mode(PhonefsodUsageService *object,
		DBusGMethodInvocation *context);

void
phonefsod_usage_service_set_default_brightness(PhonefsodUsageService *object,
					       int brightness,
					       DBusGMethodInvocation *context);
void
phonefsod_usage_service_get_default_brightness(PhonefsodUsageService *object,
					       DBusGMethodInvocation *context);

void
phonefsod_usage_service_set_pdp_credentials(PhonefsodUsageService *object,
					    const char *apn,
					    const char *user,
					    const char *password,
					    DBusGMethodInvocation *context);

PhonefsodUsageService *phonefsod_usage_service_new(void);

#endif
