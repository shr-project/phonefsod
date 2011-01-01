/*
 *  Copyright (C) 2009
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

#ifndef _PHONEFSOD_DBUS_COMMON_H
#define _PHONEFSOD_DBUS_COMMON_H

/* phonefsod */
#define PHONEFSOD_SERVICE                    "org.shr.phonefso"
#define PHONEFSOD_PATH                       "/org/shr/phonefso"

#define PHONEFSOD_USAGE_INTERFACE            PHONEFSOD_SERVICE ".Usage"
#define PHONEFSOD_USAGE_PATH                 PHONEFSOD_PATH "/Usage"

/* phoneuid */
#define PHONEUID_SERVICE                     "org.shr.phoneui"
#define PHONEUID_PATH                        "/org/shr/phoneui"

#define PHONEUID_CALL_MANAGEMENT_INTERFACE   PHONEUID_SERVICE ".CallManagement"
#define PHONEUID_DIALER_INTERFACE            PHONEUID_SERVICE ".Dialer"
#define PHONEUID_NOTIFICATION_INTERFACE      PHONEUID_SERVICE ".Notification"
#define PHONEUID_CONTACTS_INTERFACE          PHONEUID_SERVICE ".Contacts"
#define PHONEUID_MESSAGES_INTERFACE          PHONEUID_SERVICE ".Messages"
#define PHONEUID_PHONE_LOG_INTERFACE         PHONEUID_SERVICE ".PhoneLog"
#define PHONEUID_IDLE_SCREEN_INTERFACE       PHONEUID_SERVICE ".IdleScreen"

#define PHONEUID_CALL_MANAGEMENT_PATH        PHONEUID_PATH "/CallManagement"
#define PHONEUID_DIALER_PATH                 PHONEUID_PATH "/Dialer"
#define PHONEUID_NOTIFICATION_PATH           PHONEUID_PATH "/Notification"
#define PHONEUID_CONTACTS_PATH               PHONEUID_PATH "/Contacts"
#define PHONEUID_MESSAGES_PATH               PHONEUID_PATH "/Messages"
#define PHONEUID_PHONE_LOG_PATH              PHONEUID_PATH "/PhoneLog"
#define PHONEUID_IDLE_SCREEN_PATH            PHONEUID_PATH "/IdleScreen"

/* freesmartphone.org */
#define FSO_USAGE_SERVICE                    "org.freesmartphone.ousaged"
#define FSO_USAGE_PATH                       "/org/freesmartphone/Usage"
#define FSO_USAGE_IFACE                      "org.freesmartphone.Usage"

#define FSO_GSM_SERVICE                      "org.freesmartphone.ogsmd"
#define FSO_GSM_DEVICE_PATH                  "/org/freesmartphone/GSM/Device"
#define FSO_GSM_DEVICE_IFACE                 "org.freesmartphone.GSM.Device"
#define FSO_GSM_SIM_IFACE                    "org.freesmartphone.GSM.SIM"
#define FSO_GSM_NETWORK_IFACE                "org.freesmartphone.GSM.Network"
#define FSO_GSM_PDP_IFACE                    "org.freesmartphone.GSM.PDP"
#define FSO_GSM_CALL_IFACE                   "org.freesmartphone.GSM.Call"


#define FSO_PIM_SERVICE                      "org.freesmartphone.opimd"
#define FSO_PIM_MESSAGES_PATH                "/org/freesmartphone/PIM/Messages"
#define FSO_PIM_MESSAGES_IFACE               "org.freesmartphone.PIM.Messages"

#define FSO_DEVICE_SERVICE                   "org.freesmartphone.odeviced"
#define FSO_DEVICE_IDLE_NOTIFIER_PATH        "/org/freesmartphone/Device/IdleNotifier/0"
#define FSO_DEVICE_IDLE_NOTIFIER_IFACE       "org.freesmartphone.Device.IdleNotifier"
#define FSO_DEVICE_INPUT_PATH                "/org/freesmartphone/Device/Input"
#define FSO_DEVICE_INPUT_IFACE               "org.freesmartphone.Device.Input"
#define FSO_DEVICE_DISPLAY_PATH              "/org/freesmartphone/Device/Display/0"
#define FSO_DEVICE_DISPLAY_IFACE             "org.freesmartphone.Device.Display"
#define FSO_DEVICE_POWER_SUPPLY_PATH         "/org/freesmartphone/Device/PowerSupply"
#define FSO_DEVICE_POWER_SUPPLY_IFACE        "org.freesmartphone.Device.PowerSupply"


#endif
