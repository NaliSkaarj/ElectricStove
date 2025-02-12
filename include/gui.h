#ifndef _GUI_H
#define _GUI_H

#include <stdint.h>
#include "SPI.h"

#define MINUTE_TO_MILLIS(m)   ((m) * 60 * 1000)
#define HOUR_TO_MILLIS(h)     ((h) * 60 * 60 * 1000)
#define MAX_ALLOWED_TIME      ( HOUR_TO_MILLIS(99) + MINUTE_TO_MILLIS(59) )   // 100 hours max

typedef enum operationButton {
    BUTTONS_START = 1,
    BUTTONS_PAUSE_STOP,
    BUTTONS_CONTINUE_STOP,
    BUTTONS_MAX_COUNT
} buttonsGroup_t;

typedef void (* updateTimeCb)( uint32_t );
typedef void (* updateTempCb)( uint16_t );
typedef void (* operationCb)( void );
typedef void (* bakePickupCb)( uint32_t );

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
 * tabNr    - TAB's number (count from zero)
 */
void GUI_SetTabActive( uint32_t tabNr );

/**
 * Set target temperature on the screen
 * temp     - new temp. that will be shown in [°C]
 */
void GUI_SetTargetTemp( uint16_t temp );

/**
 * Set current temperature on the screen
 * temp     - new temp. that will be shown in [°C]
 */
void GUI_SetCurrentTemp( uint16_t temp );

/**
 * Set target time on the screen
 * time     - new time that will be shown in millis
 */
void GUI_SetTargetTime( uint32_t time );

/**
 * Set current time on the screen
 * time     - new time that will be shown in millis
 */
void GUI_SetCurrentTime( uint32_t time );

/**
 * Set a callback function that will be called when new time is provided by user
 * updateTimeCb     -   callback function
 */
void GUI_setTimeCallback( updateTimeCb func );

/**
 * Set a callback function that will be called when new temperature is provided by user
 * updateTempCb     -   callback function
 */
void GUI_setTempCallback( updateTempCb func );

/**
 * Set a callback function that will be called when user click on START button
 * operationCb      -   callback function
 */
void GUI_setStartCallback( operationCb func );

/**
 * Set a callback function that will be called when user click on STOP button
 * operationCb      -   callback function
 */
void GUI_setStopCallback( operationCb func );

/**
 * Set a callback function that will be called when user click on PAUSE button
 * operationCb      -   callback function
 */
void GUI_setPauseCallback( operationCb func );

/**
 * Set a callback function that will be called when user click on bake list
 * operationCb      -   callback function
 */
void GUI_setBakePickupCallback( bakePickupCb func );

/**
 * Set which group of buttons should be shown on the screen
 * btnGroup         -   BUTTON_START or BUTTON_PAUSE_STOP
 */
void GUI_setOperationButtons( enum operationButton btnGroup );

/**
 * Activate/deactivate possibility to set up time and temperature
 * active           -   whether to activate settings
 */
void GUI_setTimeTempChangeAllowed( bool active );

/**
 * Activate/deactivate label blinking (regarding TimerCurrent)
 * active           -   whether to activate blinking
 */
void GUI_setBlinkTimeCurrent( bool active );

/**
 * Get SPI instance used by TFT driver
 */
SPIClass * GUI_getSPIinstance( void );

/**
 * Show bake names as list on the screen
 * nameList         - pointer to memory where all names are (treat as elements of type char* with nameLength length)
 * nameLength       - max name length (every element on the list can be such long)
 * nameCount        - number of position on the list
 */
void GUI_populateBakeListNames( char *nameList, uint32_t nameLength, uint32_t nameCount );

#endif  // _GUI_H
