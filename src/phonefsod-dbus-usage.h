/*
 *  Copyright (C) 2008
 *      Authors (alphabetical) :
 *              Marc-Olivier Barre <marco@marcochapeau.org>
 *              Julien Cassignol <ainulindale@gmail.com>
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

void phonefsod_usage_service_get_resource_state(PhonefsodUsageService *
						 object, const char *resource,
						 DBusGMethodInvocation *
						 context);
void phonefsod_usage_service_release_resource(PhonefsodUsageService * object,
					       const char *resource,
					       DBusGMethodInvocation * context);
void phonefsod_usage_service_request_resource(PhonefsodUsageService * object,
					       const char *resource,
					       DBusGMethodInvocation * context);

PhonefsodUsageService *phonefsod_usage_service_new(void);

#endif
