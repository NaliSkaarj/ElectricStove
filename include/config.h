#ifndef _CONFIG_H_
#define _CONFIG_H_

#include "SPI.h"
#include "WString.h"

// #define BAKES_COUNT       20
#define BAKE_NAME_LENGTH  64
#define BAKE_FILE_NAME    "/bakes2.txt"

typedef char bakeName[ BAKE_NAME_LENGTH ];

enum tabs {
  TAB_MAIN = 0,
  TAB_LIST,
  TAB_OPTIONS,
  TAB_COUNT
};

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

/**
 * Get temperature for specified bake
 * idx      - index for particular bake on the list (count from 0)
 * 
 * return   - temperature
 */
uint32_t CONF_getBakeTemp( uint32_t idx );

/**
 * Get time for specified bake
 * idx      - index for particular bake on the list (count from 0)
 * 
 * return   - time (in seconds)
 */
uint32_t CONF_getBakeTime( uint32_t idx );

/**
 * Get specified bake's name
 * idx      - index for particular bake on the list (count from 0)
 * 
 * return   - pointer to name (NULL terminated)
 */
char * CONF_getBakeName( uint32_t idx );

#endif  // _CONFIG_H_
