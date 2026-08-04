#include <M5Unified.h>
#include <stdlib.h>
#include <string.h>
#include "Log.h"
#include "Printer.h"
#include "State.h"

// Slot index -> topic. Indices match Printer's reading-order layout.
const char *const TEMPERATURE_TOPICS[SLOT_COUNT] = {
    TEMPERATURE_0,
    TEMPERATURE_1,
    TEMPERATURE_2,
    TEMPERATURE_3,
};

// Drawn for a slot whose reading is missing ("-") or has gone stale. Both mean
// "there is no number here you should trust".
static const int32_t NO_VALUE_COLOR = TFT_DARKGREY;

// Parses a payload temperature. Returns false for "-" and anything else that is
// not a number, which is how a publisher signals "no reading right now".
static bool parseTemperature(const char *text, double &out)
{
  if (text == nullptr || *text == '\0')
  {
    return false;
  }
  char *end = nullptr;
  double value = strtod(text, &end);
  if (end == text)
  {
    return false;
  }
  out = value;
  return true;
}

static int32_t temperatureColor(const char *text)
{
  double value = 0;
  if (!parseTemperature(text, value))
  {
    return NO_VALUE_COLOR;
  }

  if (value >= 25)
  {
    return TFT_RED;
  }
  if (value >= 20)
  {
    return TFT_ORANGE;
  }
  if (value >= 15)
  {
    return TFT_GREENYELLOW;
  }
  if (value >= 10)
  {
    return TFT_GREEN;
  }
  if (value >= 5)
  {
    return TFT_CYAN;
  }
  if (value >= 0)
  {
    return TFT_LIGHTGREY;
  }
  return TFT_WHITE; // Below freezing.
}

State::State(Printer *printer) : _printer(printer)
{
  memset(_slots, 0, sizeof(_slots));
}

bool State::update(const char *topic, const uint8_t *payload, unsigned int length)
{
  for (uint8_t i = 0; i < SLOT_COUNT; i += 1)
  {
    if (strcmp(topic, TEMPERATURE_TOPICS[i]) == 0)
    {
      apply(i, payload, length);
      return true;
    }
  }

  if (strcmp(topic, SLEEP) == 0)
  {
    LOG("state: sleep");
    M5.Display.sleep();
    return true;
  }

  if (strcmp(topic, WAKE) == 0)
  {
    LOG("state: wake");
    M5.Display.wakeup();
    redraw();
    return true;
  }

  LOG("state: unhandled topic %s", topic);
  return false;
}

void State::apply(uint8_t index, const uint8_t *payload, unsigned int length)
{
  Slot &slot = _slots[index];

  char temperature[BUFFER_SIZE];
  char name[BUFFER_SIZE];
  split(payload, length, temperature, name);

  LOG("slot %u: '%s' = '%s'", index, name, temperature);

  bool unchanged = slot.received &&
                   !slot.stale &&
                   strcmp(temperature, slot.temperature) == 0 &&
                   strcmp(name, slot.name) == 0;

  slot.lastUpdate = millis();
  slot.received = true;
  slot.stale = false;

  // Repainting is a visible flash, so skip it when the sensor just republished
  // the same reading - which, with retained messages and a periodic publisher,
  // is most of the traffic.
  if (unchanged)
  {
    return;
  }

  memcpy(slot.temperature, temperature, BUFFER_SIZE);
  memcpy(slot.name, name, BUFFER_SIZE);
  render(index);
}

void State::tick()
{
#if STALE_TIMEOUT_MS > 0
  uint32_t now = millis();
  for (uint8_t i = 0; i < SLOT_COUNT; i += 1)
  {
    Slot &slot = _slots[i];
    // Unsigned subtraction, so this stays correct across the millis() rollover
    // at ~49 days of uptime.
    if (slot.received && !slot.stale && (now - slot.lastUpdate) > STALE_TIMEOUT_MS)
    {
      LOG("slot %u: stale after %lu ms", i, (unsigned long)(now - slot.lastUpdate));
      slot.stale = true;
      render(i);
    }
  }
#endif
}

void State::redraw()
{
  // The panel may have lost its pixels across a sleep/wake, so the status dot
  // has to be repainted too even though its colour has not changed. loop()
  // repaints it on the next iteration.
  _printer->invalidateDot();
  for (uint8_t i = 0; i < SLOT_COUNT; i += 1)
  {
    render(i);
  }
}

int32_t State::colorFor(const Slot &slot) const
{
  if (slot.stale || !slot.received)
  {
    return NO_VALUE_COLOR;
  }
  return temperatureColor(slot.temperature);
}

void State::render(uint8_t index)
{
  const Slot &slot = _slots[index];
  _printer->slot(index, slot.name, slot.temperature, colorFor(slot));
}

void State::split(const uint8_t *source, unsigned int length, char *first, char *second)
{
  memset(first, '\0', BUFFER_SIZE);
  memset(second, '\0', BUFFER_SIZE);

  // Everything before the first delimiter goes to `first`, the rest to
  // `second`. Only the first delimiter splits; any later ones belong to
  // `second`. A payload can be longer than BUFFER_SIZE (PubSubClient allows
  // 256), so each side is truncated at BUFFER_SIZE - 1 to keep room for the
  // null terminator.
  char *target = first;
  unsigned int written = 0;

  for (unsigned int i = 0; i < length; i += 1)
  {
    char current = (char)source[i];
    if (current == PAYLOAD_DELIMITER && target == first)
    {
      target = second;
      written = 0;
      continue;
    }

    if (written < BUFFER_SIZE - 1)
    {
      target[written] = current;
      written += 1;
    }
  }
}
