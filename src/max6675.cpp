// this library is public domain. enjoy!
// https://learn.adafruit.com/thermocouple/

#include "max6675.h"
#include "SPI.h"

SPIClass spi2 = SPIClass(VSPI);
/**************************************************************************/
/*!
    @brief  Initialize a MAX6675 sensor
    @param   SCLK The Arduino pin connected to Clock
    @param   CS The Arduino pin connected to Chip Select
    @param   MISO The Arduino pin connected to Data Out
*/
/**************************************************************************/
MAX6675::MAX6675( int8_t SCLK=-1, int8_t CS=-1, int8_t MISO=-1 ) {
  cs = CS;

  pinMode( cs, OUTPUT );
  digitalWrite( cs, HIGH );

  spi2.begin( SCLK, MISO, MOSI, cs );
  // spi2.setHwCs( true );
  // spi2.setHwCs( false );
  // pinMode( cs, OUTPUT );
  // // digitalWrite( cs, HIGH );
}

/**************************************************************************/
/*!
    @brief  Read the Celsius temperature
    @returns Temperature in C or NAN on failure!
*/
/**************************************************************************/
float MAX6675::readCelsius( void ) {

  uint16_t v;

  spi2.beginTransaction( SPISettings( 20000000, MSBFIRST, 0 ) );

  digitalWrite( cs, LOW );
  v = spi2.transfer16( 0x00 );
  digitalWrite( cs, HIGH );

  spi2.endTransaction();

  if ( v & 0x4 ) {
    // no thermocouple attached!
    return NAN;
  }

  v >>= 3;

  return v * 0.25;
}
