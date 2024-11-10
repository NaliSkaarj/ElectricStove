#include "Arduino.h"
#include "max6675.h"
#include "SPI.h"

static int8_t sclk;
static int8_t miso;
static int8_t cs;
static bool   initialized = false;

SPIClass mySpi = SPIClass( VSPI );

void MAX6675_Init( int8_t _SCLK, int8_t _CS, int8_t _MISO ) {
  if( true == initialized ) {
    return;
  }

  if( 0 >= _SCLK || 0 >= _CS || 0 >= _MISO ) {
    return;
  }

  cs = _CS;

  pinMode( cs, OUTPUT );
  digitalWrite( cs, HIGH );

  mySpi.begin( _SCLK, _MISO, MOSI, cs );

  initialized = true;
}

float MAX6675_readCelsius( void ) {
  uint16_t v;

  if( false == initialized ) {
    return NAN;
  }

  mySpi.beginTransaction( SPISettings( 20000000, MSBFIRST, 0 ) );

  digitalWrite( cs, LOW );
  v = mySpi.transfer16( 0x00 );
  digitalWrite( cs, HIGH );

  mySpi.endTransaction();

  if ( v & 0x4 ) {
    // no thermocouple attached!
    return NAN;
  }

  v >>= 3;

  return v * 0.25;
}
