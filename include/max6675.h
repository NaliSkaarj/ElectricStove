#ifndef _MAX6675_H
#define _MAX6675_H

/**
 * Need to be called from main Setup/Init function to run the service
 * _SCLK      - SPI clock pin
 * _CS        - SPI chip select pin
 * _MISO      - SPI MISO pin
 */
void MAX6675_Init( int8_t _SCLK, int8_t _CS, int8_t _MISO );

/**
 * Read temperature
 * return (float)   - temperature in celsius or NAN on failure
 */
float MAX6675_readCelsius( void );

#endif  // _MAX6675_H