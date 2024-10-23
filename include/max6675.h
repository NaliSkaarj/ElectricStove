// this library is public domain. enjoy!
// https://learn.adafruit.com/thermocouple/

#ifndef _MAX6675_H_
#define _MAX6675_H_

#include "Arduino.h"

/**************************************************************************/
/*!
    @brief  Class for communicating with thermocouple sensor
*/
/**************************************************************************/
class MAX6675 {
public:
  MAX6675(int8_t SCLK, int8_t CS, int8_t MISO);

  float readCelsius(void);

private:
  int8_t cs;
};

#endif  // _MAX6675_H_
