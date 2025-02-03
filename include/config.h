#ifndef _CONFIG_H_
#define _CONFIG_H_

#include "SPI.h"
#include "WString.h"

#define BAKES_COUNT       5
#define BAKE_NAME_LENGTH  64

typedef char bakeName[ BAKE_NAME_LENGTH ];

enum options {
  BUZZER_MENU = 0,
  BUZZER_HEATING,
  OTA_ACTIVE,
  OPTIONS_COUNT
};

typedef struct
{
  bakeName  name;
  uint32_t  temp;
  uint32_t  time;
} bake_t;

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

/**
 * Get all names from bake list
 * array    - pointer to dynamically allocated memory where bake names will be stored
 * cnt      - number of elements in allocated memory
 */
void CONF_getBakeNames( bakeName **bList, uint32_t *cnt );

#endif  // _CONFIG_H_
