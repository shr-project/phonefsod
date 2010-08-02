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

#ifndef _PHONEFSOD_FSO_H
#define _PHONEFSOD_FSO_H

#include <glib.h>

gboolean fso_init();
void fso_connect_usage();
void fso_connect_gsm();
void fso_connect_pim();
void fso_connect_device();

gboolean fso_startup();
void fso_dimit(int percent, int dim);
void fso_get_resource_state(const char *resource, void (*callback)(GError *, gboolean, gpointer), gpointer data);
gboolean fso_set_functionality();
void fso_pdp_set_credentials();

#endif
