// Template for src/overrides.h. Copy it and edit what you need:
//
//   cp src/overrides.example.h src/overrides.h
//
// src/overrides.h is gitignored (.gitignore) and entirely optional - without it
// every value below applies as the default from src/Config.h.
//
// The file is *partial*: define only the macros you actually want to change and
// the rest fall back to src/Config.h. You can delete every line you are not
// overriding.

#ifndef LOCAL_OVERRIDES_H
#define LOCAL_OVERRIDES_H

// --- Temperature topics, one per quadrant -------------------------------
// Payload format: "<temperature>|<name>", e.g. "-3.5|Kitchen" or "-|Backside".
#define TEMPERATURE_0 "m5/temperature/0" // Upper left
#define TEMPERATURE_1 "m5/temperature/1" // Upper right
#define TEMPERATURE_2 "m5/temperature/2" // Lower left
#define TEMPERATURE_3 "m5/temperature/3" // Lower right

// --- Control topics -----------------------------------------------------
#define REQUEST_UPDATE "m5/request/update" // Published by the M5, payload "true"
#define SLEEP "m5/status/sleep"            // Subscribed: blanks the display
#define WAKE "m5/status/wake"              // Subscribed: wakes the display

// --- Payload parsing ----------------------------------------------------
// Capacity of each name/temperature buffer, including the null terminator.
// Longer payload halves are truncated to BUFFER_SIZE - 1 characters.
#define BUFFER_SIZE 50
#define PAYLOAD_DELIMITER '|'

#endif
