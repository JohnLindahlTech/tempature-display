#include <M5Unified.h>
#include "Printer.h"
#include "State.h"

uint32_t getTemperatureColor(char *temp)
{
  double num = atof(temp);
  if (num >= 25)
  {
    return TFT_RED; // RED OK
  }
  else if (num >= 20)
  {
    return TFT_ORANGE; // ORANGE OK
  }
  else if (num >= 15)
  {
    return TFT_GREENYELLOW; // DARKGREEN BAD
  }
  else if (num >= 10)
  {
    return TFT_GREEN; // GREEN; OK
  }
  else if (num >= 5)
  {
    return TFT_CYAN; // CYAN; OK
  }
  else if (num >= 0)
  {
    return TFT_LIGHTGREY;
  }
  return TFT_WHITE; // WHITE; TBD
}

State::State(Printer *printer)
{
  _printer = printer;
}

void State::update(char *topic, uint8_t *payload, unsigned int length)
{

  if (strcmp(topic, TEMPERATURE_0) == 0)
  {
    split((char *)payload, length, _upperLeftTemp, _upperLeftName);
    _printer->upperLeft(_upperLeftName, _upperLeftTemp, getTemperatureColor(_upperLeftTemp));
  }

  if (strcmp(topic, TEMPERATURE_1) == 0)
  {
    split((char *)payload, length, _upperRightTemp, _upperRightName);
    _printer->upperRight(_upperRightName, _upperRightTemp, getTemperatureColor(_upperRightTemp));
  }

  if (strcmp(topic, TEMPERATURE_2) == 0)
  {
    split((char *)payload, length, _lowerLeftTemp, _lowerLeftName);
    _printer->lowerLeft(_lowerLeftName, _lowerLeftTemp, getTemperatureColor(_lowerLeftTemp));
  }

  if (strcmp(topic, TEMPERATURE_3) == 0)
  {
    split((char *)payload, length, _lowerRightTemp, _lowerRightName);
    _printer->lowerRight(_lowerRightName, _lowerRightTemp, getTemperatureColor(_lowerRightTemp));
  }

  if (strcmp(topic, SLEEP) == 0)
  {
    M5.Display.sleep();
  }

  if (strcmp(topic, WAKE) == 0)
  {
    M5.Display.wakeup();
  }

  _printer->dot(M5.Display.getBaseColor());
}

void State::split(char source[], unsigned int length, char t1[], char t2[])
{
  int foundDelimiter = -1;
  char posChar = '\0';
  memset(t1, '\0', BUFFER_SIZE);
  memset(t2, '\0', BUFFER_SIZE);

  for (unsigned int i = 0; i < length; i += 1)
  {
    posChar = (char)source[i];
    if (posChar == PAYLOAD_DELIMITER)
    {
      foundDelimiter = i;
    }
    else if (foundDelimiter >= 0)
    {
      t2[i - (foundDelimiter + 1)] = posChar;
    }
    else
    {
      t1[i] = posChar;
    }
  }
}
