#ifndef _GUI_H
#define _GUI_H

#include <stdint.h>

/**
 * Need to be called from main Setup/Init function to run the service
 */
void GUI_Init();

/**
 * Need to be called in loop() to run the service
 * tick_period  - the call period of this function in milliseconds
 */
void GUI_Handle( uint32_t tick_period );

/**
 * Which TAB to show on the screen
 * tabNr  - TAB's number (count from zero)
 */
void GUI_SetTabActive( uint32_t tabNr );

#endif  // _GUI_H
