#ifndef _CONFIG_H_
#define _CONFIG_H_

#include "SPI.h"

enum options {
  BUZZER_MENU = 0,
  BUZZER_HEATING,
  OTA_ACTIVE,
  OPTIONS_COUNT
};

/**
 * Need to be called from main Setup/Init function to run the service
 * spi      - SPI instance for SDCard operations
 */
void CONF_Init( SPIClass * spi );

/**
 * Get specific configuration option
 * option   - which options you're interested in
 * 
 * return   - options value or
 *            -1 when failed
 */
int CONF_getOption( enum options option );

/**
 * Set specific configuration option value
 * option   - which options you're interested in
 * val      - new value to be set
 */
void CONF_setOption( enum options option, int val );

#endif  // _CONFIG_H_
