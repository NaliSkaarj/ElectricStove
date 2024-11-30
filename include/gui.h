#ifndef _GUI_H
#define _GUI_H

#include <stdint.h>

enum operationButton {
    BUTTON_START = 1,
    BUTTON_PAUSE_STOP,
    BUTTON_MAX_COUNT
};

typedef void (* updateTimeCb)( uint32_t );
typedef void (* updateTempCb)( float );
typedef void (* operationCb)( void );

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
 * Set which group of buttons should be shown on the screen
 * btnGroup         -   BUTTON_START or BUTTON_PAUSE_STOP
 */
void GUI_setOperationButtons( enum operationButton btnGroup );

#endif  // _GUI_H
