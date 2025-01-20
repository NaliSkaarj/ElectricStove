#ifndef _MAX6675_H
#define _MAX6675_H

#include "SPI.h"

#define TEMP_READ_INTERVAL      1000        // in millis
#define MAX6675_STACK_SIZE      1536
#define MAX6675_TASK_PRIORITY   3

/**
 * Need to be called from main Setup/Init function to run the service
 * spi          - SPI instance which will be used for communication
 * _CS          - SPI chip select pin
 */
void MAX6675_Init( SPIClass * spi, int8_t _CS );

/**
 * Read temperature
 * return (float)   - temperature in celsius or NAN on failure
 */
float MAX6675_readCelsius( void );

#endif  // _MAX6675_H
