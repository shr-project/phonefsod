#ifndef _PHONEFSOD_DBUS_COMMON_H
#define _PHONEFSOD_DBUS_COMMON_H

/* phonefsod */
#define PHONEFSOD_SERVICE                    "org.shr.phonefso"
#define PHONEFSOD_PATH                       "/org/shr/phonefso"

#define PHONEFSOD_USAGE_NAME                 PHONEFSOD_SERVICE ".Usage"
#define PHONEFSOD_USAGE_INTERFACE            PHONEFSOD_SERVICE ".Usage"
#define PHONEFSOD_USAGE_PATH                 PHONEFSOD_PATH "/Usage"


/* phoneuid */
#define PHONEUID_SERVICE                     "org.shr.phoneui"
#define PHONEUID_PATH                        "/org/shr/phoneui"

#define PHONEUID_CALL_MANAGEMENT_NAME        PHONEUID_SERVICE ".CallManagement"
#define PHONEUID_DIALER_NAME                 PHONEUID_SERVICE ".Dialer"
#define PHONEUID_NOTIFICATION_NAME           PHONEUID_SERVICE ".Notification"
#define PHONEUID_CONTACTS_NAME               PHONEUID_SERVICE ".Contacts"
#define PHONEUID_MESSAGES_NAME               PHONEUID_SERVICE ".Messages"
#define PHONEUID_PHONE_LOG_NAME              PHONEUID_SERVICE ".PhoneLog"

#define PHONEUID_CALL_MANAGEMENT_INTERFACE   PHONEUID_SERVICE ".CallManagement"
#define PHONEUID_DIALER_INTERFACE            PHONEUID_SERVICE ".Dialer"
#define PHONEUID_NOTIFICATION_INTERFACE      PHONEUID_SERVICE ".Notification"
#define PHONEUID_CONTACTS_INTERFACE          PHONEUID_SERVICE ".Contacts"
#define PHONEUID_MESSAGES_INTERFACE          PHONEUID_SERVICE ".Messages"
#define PHONEUID_PHONE_LOG_INTERFACE         PHONEUID_SERVICE ".PhoneLog"

#define PHONEUID_CALL_MANAGEMENT_PATH        PHONEUID_PATH "/CallManagement"
#define PHONEUID_DIALER_PATH                 PHONEUID_PATH "/Dialer"
#define PHONEUID_NOTIFICATION_PATH           PHONEUID_PATH "/Notification"
#define PHONEUID_CONTACTS_PATH               PHONEUID_PATH "/Contacts"
#define PHONEUID_MESSAGES_PATH               PHONEUID_PATH "/Messages"
#define PHONEUID_PHONE_LOG_PATH              PHONEUID_PATH "/PhoneLog"

#endif
