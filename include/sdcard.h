#ifndef _SDCARD_H_
#define _SDCARD_H_

#include "SD.h"

#define SD_CS 14

void SDCARD_Setup( SPIClass * spi );
void SDCARD_log();

#endif  // _SDCARD_H_
