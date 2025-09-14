#ifndef _CONFIG_H_
#define _CONFIG_H_

#include "SPI.h"
#include "WString.h"

// #define BAKES_COUNT       20
#define BAKE_NAME_LENGTH    64
#define BAKE_FILE_NAME      "/bakes.txt"
#define BAKE_MAX_STEPS      10    // how much steps can be in one 'bakes curve'

typedef char bakeName[ BAKE_NAME_LENGTH ];

typedef struct
{
  int32_t  temp;
  int32_t  time;
} bakeStep_t;

typedef struct
{
  bakeName    name;
  uint32_t    stepCount;
  bakeStep_t  step[ BAKE_MAX_STEPS ];
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
 * return   - options value
 */
int CONF_getOptionInt( int32_t option );
bool CONF_getOptionBool( int32_t option );

/**
 * Set specific configuration option
 * option   - which options you're interested in (EEPROM address)
 * value    - value to be stored
 */
void CONF_setOptionBool( int32_t option, bool value );
void CONF_setOptionInt( int32_t option, int32_t value );

/**
 * Get all names from bake list
 * array    - pointer to dynamically allocated memory where bake names will be stored
 * cnt      - number of elements in allocated memory
 */
void CONF_getBakeNames( bakeName **bList, uint32_t *cnt );

/**
 * Get temperature for specified bake
 * idx      - index for particular bake on the list (count from 0)
 * step     - step for which value will be returned (count from 0)
 * 
 * return   - temperature
 */
uint32_t CONF_getBakeTemp( uint32_t idx, uint32_t step );

/**
 * Get time for specified bake
 * idx      - index for particular bake on the list (count from 0)
 * step     - step for which value will be returned (count from 0)
 * 
 * return   - time (in seconds), negative value mean special event
 */
int32_t CONF_getBakeTime( uint32_t idx, uint32_t step );

/**
 * Get steps count for specified bake
 * idx      - index for particular bake on the list (count from 0)
 * 
 * return   - steps count
 */
uint32_t CONF_getBakeStepCount( uint32_t idx );

/**
 * Get specified bake's name
 * idx      - index for particular bake on the list (count from 0)
 * 
 * return   - pointer to name (NULL terminated)
 */
char * CONF_getBakeName( uint32_t idx );

/**
 * Get specified bake's data as serialized string
 * idx      - index for particular bake on the list (count from 0)
 * 
 * return   - pointer to name (NULL terminated), need to be free()
 */
char * CONF_getBakeSerializedData( uint32_t idx );

/**
 * Remove bakes from the list
 * list[]   - array with indexes (which bake positions on the list should be removed, count from 0)
 * 
 * return   - true if bakes found on the list and removed
 */
bool CONF_removeBakes( uint8_t list[], uint32_t count );

/**
 * Swap two bakes on the list
 * list[]   - array with indexes (which bake positions on the list should be swapped, count from 0)
 * 
 * return   - true if bakes found on the list and swapped
 */
bool CONF_swapBakes( uint8_t list[] );

/**
 * Add bakes from file to current list
 */
void CONF_addBakesFromFile( void );

/**
 * Save bake list as file on spiffs partition
 */
void CONF_storeBakeList( void );

#endif  // _CONFIG_H_
