#pragma once

// Every compile-time setting for the display lives here.
//
// Two optional, gitignored files let you override things locally:
//
//   src/credentials.h - WiFi, broker address, login, CA certificate
//   src/overrides.h   - topics, timings, layout
//
// Both are *partial*: define only the macros you actually want to change and
// everything else falls back to the default below. (The old behaviour required
// overrides.h to redefine every single macro, so adding a setting upstream
// broke every existing checkout.)

#if defined __has_include
#if __has_include("credentials.h")
#include "credentials.h"
#define HAVE_CREDENTIALS_H 1
#endif
#if __has_include("overrides.h")
#include "overrides.h"
#endif
#endif

#ifndef HAVE_CREDENTIALS_H
#warning "src/credentials.h not found - building with placeholder WiFi/MQTT settings. See README.md."
#endif

// --- Network ---------------------------------------------------------------

// WIFI_SSID, not SSID. The Arduino core's WiFi.h declares
// WiFiSTAClass::SSID(), so a `#define SSID "..."` reaching the preprocessor
// first rewrites that declaration into a string literal - and the compiler then
// reports a syntax error inside WiFi.h, with nothing visibly wrong at the point
// of use. The short name is deliberately not accepted; see the #error below.
#ifndef WIFI_SSID
#define WIFI_SSID "Wifi-SSID"
#endif
#ifndef WIFI_PASSPHRASE
#define WIFI_PASSPHRASE "Wifi-passphrase"
#endif

// Turns the inscrutable failure above into a one-line diagnostic. This can only
// trip if credentials.h or overrides.h defines SSID as a macro: by the time any
// translation unit reaches here having already included WiFi.h, SSID is a member
// function name and not a macro at all, so there are no false positives.
#ifdef SSID
#error "Rename SSID to WIFI_SSID in src/credentials.h - SSID collides with WiFiSTAClass::SSID in the Arduino core."
#endif

// NTP is not cosmetic: mbedTLS can only check the broker certificate's
// notBefore/notAfter dates once the clock is set. Without it the device boots
// at 1970 and the validity window is effectively unverified.
#ifndef NTP_SERVER
#define NTP_SERVER "pool.ntp.org"
#endif

// --- Broker ----------------------------------------------------------------

#ifndef MQTT_SERVER
#define MQTT_SERVER "mqtt.home.arpa"
#endif
#ifndef MQTT_PORT
#define MQTT_PORT 8883
#endif
#ifndef MQTT_USER
#define MQTT_USER "user"
#endif
#ifndef MQTT_PASSWORD
#define MQTT_PASSWORD "password"
#endif
// Spelled MQTT_CLIENT_IT in older credentials.h files; both are accepted.
#if !defined(MQTT_CLIENT_ID) && defined(MQTT_CLIENT_IT)
#define MQTT_CLIENT_ID MQTT_CLIENT_IT
#endif
#ifndef MQTT_CLIENT_ID
#define MQTT_CLIENT_ID "myMqttClient"
#endif

// Notice the format for each row: inside the string use \n, and the end of each
// line needs a \ (backslash).
#ifndef CA_CERT
#define CA_CERT "-----BEGIN CERTIFICATE-----\n" \
                "asfefgwgegwegeg\n"             \
                "-----END CERTIFICATE-----"
#endif

// --- Topics ----------------------------------------------------------------
//
// One topic per screen quadrant. Payload format is "<temperature>|<name>",
// e.g. "-3.5|Kitchen", or "-|Backside" for a sensor with no current reading.
//
// Publish these RETAINED. The broker then replays the last value the moment the
// display subscribes, which is what makes the display correct after a reboot or
// a WiFi drop. The old "m5/request/update" poll existed only to work around
// non-retained publishing and has been removed.

#ifndef TEMPERATURE_0
#define TEMPERATURE_0 "m5/temperature/0" // Upper left
#endif
#ifndef TEMPERATURE_1
#define TEMPERATURE_1 "m5/temperature/1" // Upper right
#endif
#ifndef TEMPERATURE_2
#define TEMPERATURE_2 "m5/temperature/2" // Lower left
#endif
#ifndef TEMPERATURE_3
#define TEMPERATURE_3 "m5/temperature/3" // Lower right
#endif

#ifndef SLEEP
#define SLEEP "m5/status/sleep" // Subscribed: blanks the display
#endif
#ifndef WAKE
#define WAKE "m5/status/wake" // Subscribed: wakes the display
#endif

// Last Will and Testament. The display publishes "online" (retained) once it
// connects; the broker publishes "offline" (retained) on its behalf if the
// connection dies without a clean DISCONNECT.
#ifndef AVAILABILITY_TOPIC
#define AVAILABILITY_TOPIC "m5/status/availability"
#endif
#ifndef AVAILABILITY_ONLINE
#define AVAILABILITY_ONLINE "online"
#endif
#ifndef AVAILABILITY_OFFLINE
#define AVAILABILITY_OFFLINE "offline"
#endif

// --- OTA -------------------------------------------------------------------
//
// The board's default partition table (default_16MB.csv) already provides two
// 6.25 MB app slots plus otadata, so over-the-air updates need no flash layout
// change. Upload over the air with the dedicated env in platformio.ini:
//
//   pio run -e m5stack-core2-ota -t upload
//
// The first flash must still go over USB - see "OTA updates" in README.md.

#ifndef OTA_ENABLED
#define OTA_ENABLED 1
#endif

// Also the mDNS name, so the device answers at <hostname>.local.
#ifndef OTA_HOSTNAME
#define OTA_HOSTNAME "m5-temperature-display"
#endif

#ifndef OTA_PORT
#define OTA_PORT 3232
#endif

// How long a freshly flashed image must run before it is confirmed good and the
// pending rollback is cancelled. Nothing is blocked or delayed while this runs:
// the display and the broker behave exactly as normal, and the only deferred
// action is a one-time flag write. It also only ever applies to the first boot
// after an OTA push - a normal reboot, or a serial flash, is unaffected.
//
// The trade-off is at the two ends. Too short and firmware that panics a few
// seconds into loop() gets confirmed before it fails. Too long and an ordinary
// power cut during the window rolls back an image that was fine - this display
// runs on grid power with no battery, so that is a real if unlikely event.
// Set to 0 to confirm as soon as the OTA listener is up.
#ifndef OTA_VALIDATE_AFTER_MS
#define OTA_VALIDATE_AFTER_MS 60000UL
#endif

// Anyone on the LAN can push firmware to an unauthenticated listener. Set this
// in src/credentials.h alongside the WiFi passphrase.
#ifndef OTA_PASSWORD
#define OTA_PASSWORD ""
#if OTA_ENABLED
#warning "OTA_PASSWORD not set - over-the-air updates are unauthenticated. Define it in src/credentials.h."
#endif
#endif

// --- Payload parsing -------------------------------------------------------

// Capacity of each name/temperature buffer, including the null terminator.
// Longer payload halves are truncated to BUFFER_SIZE - 1 characters.
#ifndef BUFFER_SIZE
#define BUFFER_SIZE 50
#endif
#ifndef PAYLOAD_DELIMITER
#define PAYLOAD_DELIMITER '|'
#endif

// --- Timings ---------------------------------------------------------------

// How long to wait between WiFi / MQTT reconnect attempts.
#ifndef WIFI_RETRY_INTERVAL_MS
#define WIFI_RETRY_INTERVAL_MS 5000
#endif
#ifndef MQTT_RETRY_INTERVAL_MS
#define MQTT_RETRY_INTERVAL_MS 5000
#endif

// MQTT keepalive. The broker declares the display dead - and fires the Last
// Will - after roughly 1.5x this.
#ifndef MQTT_KEEPALIVE_SECONDS
#define MQTT_KEEPALIVE_SECONDS 30
#endif

// How long the centre dot flashes when an MQTT message arrives. The dot is a
// traffic indicator, not a status light: while connected and idle it stays dark
// and only blips on activity. Fault colours (see StickyWiFi/MQTT statusColor)
// are persistent, so a problem stays visible.
#ifndef ACTIVITY_BLIP_MS
#define ACTIVITY_BLIP_MS 250
#endif

// A quadrant that has not received a message for this long is drawn greyed out,
// so a dead sensor is visibly dead instead of showing a stale number forever.
// Set to 0 to disable staleness entirely.
#ifndef STALE_TIMEOUT_MS
#define STALE_TIMEOUT_MS (90UL * 60UL * 1000UL)
#endif

// --- Debug -----------------------------------------------------------------

// Serial logging. Set -D DEBUG=0 in platformio.ini to compile it all out.
#ifndef DEBUG
#define DEBUG 1
#endif
#ifndef SERIAL_BAUD
#define SERIAL_BAUD 115200
#endif
