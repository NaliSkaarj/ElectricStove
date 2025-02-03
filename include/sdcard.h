#ifndef _SDCARD_H_
#define _SDCARD_H_

#include "SD.h"

#define SD_CS 14

void SDCARD_Setup( SPIClass * spi );
void SDCARD_log();
void SDCARD_list();

/**
 * Read file <path> and return its content
 * path   - path to file
 * 
 * return - file content as integer (1B values only) or -1 on error
 */
int SDCARD_readFile( const char * path );

/**
 * Write new content to file <path>
 * path   - path to file
 * value  - value to be writen as file content
 */
void SDCARD_writeFile( const char * path, int value );

/**
 * Write content to file
 * path     - path to file
 * msg      - constant char to be writen as file content
 */
void SDCARD_writeFile( const char * path, const char * msg );

/**
 * Read content file to buffer
 * path     - path to a file
 * buf      - pointer to buffer
 * len      - buffer length
 * 
 * return   - read bytes
 */
uint32_t SDCARD_readFileContent( const char * path, uint8_t * buf, size_t len );

#endif  // _SDCARD_H_
