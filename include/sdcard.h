#ifndef _SDCARD_H_
#define _SDCARD_H_

#include "SD.h"

#define SD_CS 14

void SDCARD_Setup( SPIClass * spi );
bool SDCARD_Reinit();
void SDCARD_Eject();
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
 * Read file content to dynamically allocated buffer and return pointer to that buffer with it's size
 * path     - path to a file
 * buf      - pointer to pointer to buffer
 * 
 * return   - read bytes from file
 */
uint32_t SDCARD_getFileContent( const char * path, uint8_t ** buf );

#endif  // _SDCARD_H_
