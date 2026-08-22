// Template for src/credentials.h. Copy it, then replace every value:
//
//   cp src/credentials.example.h src/credentials.h
//
// src/credentials.h is gitignored (.gitignore) and never committed. This file
// is committed, so it must never contain a real secret.
//
// Only LOG_NOISE_PSK is mandatory. Every other macro here has a placeholder
// default in src/Config.h, which means a missing one does NOT fail the build -
// the firmware compiles, flashes, and then simply fails to connect with nothing
// in the log explaining why. Fill in all of them.

#ifndef LOCAL_CREDENTIALS_H
#define LOCAL_CREDENTIALS_H

// --- OTA ------------------------------------------------------------------
// Empty disables authentication on the OTA port; OTA.cpp warns at boot if so.
#define OTA_PASSWORD "TODO-ota-password"

// --- Encrypted log stream -------------------------------------------------
// REQUIRED - there is no unauthenticated fallback. Leave this undefined and the
// listener is not compiled in at all, so the port does not exist.
// Generate with:  openssl rand -base64 32
// Must be exactly 44 base64 characters (32 bytes); a static_assert in Log.cpp
// checks this at build time. Export the same value for the client:
//   export LOG_NOISE_PSK='<same value>'
#define LOG_NOISE_PSK "TODO-44-characters-of-base64-ending-in-an="

// --- WiFi -----------------------------------------------------------------
// Prefixed names: plain SSID / PASSPHRASE collide with WiFi.h.
#define WIFI_SSID "TODO-ssid"
#define WIFI_PASSPHRASE "TODO-passphrase"

// --- MQTT broker ----------------------------------------------------------
#define MQTT_SERVER "TODO-broker.example.com"
#define MQTT_PORT 8883
#define MQTT_USER "TODO-user"
#define MQTT_PASSWORD "TODO-password"

// Config.h accepts either spelling; MQTT_CLIENT_ID is preferred, and
// MQTT_CLIENT_IT is kept working for older credentials.h files.
#define MQTT_CLIENT_ID "m5-temperature-display"

// --- Broker CA certificate ------------------------------------------------
// Pin the ROOT, not the leaf or intermediate: the broker's leaf renews
// regularly and chains up to the root. For Let's Encrypt brokers that is
// ISRG Root X1.
// Formatting rules: a \n inside each string, and a trailing \ on every line
// but the last. See the CA_CERT section of README.md for a worked example.
#define CA_CERT "-----BEGIN CERTIFICATE-----\n" \
                "TODO-base64-of-the-root-certificate\n"  \
                "-----END CERTIFICATE-----\n"

#endif
