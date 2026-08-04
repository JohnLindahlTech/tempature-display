#pragma once

#include <Arduino.h>
#include "Config.h"
#include "Printer.h"

// Slot index -> temperature topic, in Printer's reading-order layout.
extern const char *const TEMPERATURE_TOPICS[SLOT_COUNT];

// Holds the last value for each of the four quadrants and decides when the
// screen actually needs repainting.
class State
{
public:
  State(Printer *printer);

  // Feeds one MQTT message in. Returns true if the topic was recognised.
  bool update(const char *topic, const uint8_t *payload, unsigned int length);

  // Call from loop(): greys out slots that have gone quiet.
  void tick();

  // Repaints every slot, e.g. after the display wakes up.
  void redraw();

private:
  struct Slot
  {
    char name[BUFFER_SIZE];
    char temperature[BUFFER_SIZE];
    uint32_t lastUpdate; // millis() when the last message arrived
    bool received;       // has this slot ever had a message?
    bool stale;          // currently drawn greyed out
  };

  void apply(uint8_t index, const uint8_t *payload, unsigned int length);
  void render(uint8_t index);
  int32_t colorFor(const Slot &slot) const;

  // Splits "<temperature>|<name>" into two null-terminated buffers of
  // BUFFER_SIZE bytes each.
  static void split(const uint8_t *source, unsigned int length, char *first, char *second);

  Printer *_printer;
  Slot _slots[SLOT_COUNT];
};
