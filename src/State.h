#include "Arduino.h"
#include "Printer.h"
#ifndef state_h
#define state_h

#if defined __has_include
#if __has_include("overrides.h")
// If using the overrides.h file, all the #defines from below needs to be available.
#include "overrides.h"

#else

// Upper Left
#define TEMPERATURE_0 "m5/temperature/0"

// Upper Right
#define TEMPERATURE_1 "m5/temperature/1"

// Lower Left
#define TEMPERATURE_2 "m5/temperature/2"

// Lower Right
#define TEMPERATURE_3 "m5/temperature/3"

#define REQUEST_UPDATE "m5/request/update"
#define SLEEP "m5/status/sleep"
#define WAKE "m5/status/wake"

#define BUFFER_SIZE 50
#define PAYLOAD_DELIMITER '|'

#endif
#endif

class State
{
public:
  State(Printer *printer);
  void update(char *topic, uint8_t *payload, unsigned int length);

private:
  Printer *_printer;

  char _nameBuffer[BUFFER_SIZE];
  char _tempBuffer[BUFFER_SIZE];

  char _upperLeftName[BUFFER_SIZE];
  char _upperLeftTemp[BUFFER_SIZE];

  char _upperRightName[BUFFER_SIZE];
  char _upperRightTemp[BUFFER_SIZE];

  char _lowerLeftName[BUFFER_SIZE];
  char _lowerLeftTemp[BUFFER_SIZE];

  char _lowerRightName[BUFFER_SIZE];
  char _lowerRightTemp[BUFFER_SIZE];

  void split(char source[], unsigned int length, char t1[], char t2[]);
};

#endif
