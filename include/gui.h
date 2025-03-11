#ifndef _GUI_H
#define _GUI_H

#include <stdint.h>
#include "SPI.h"
#include "lvgl.h"

#define BAKES_TO_REMOVE_MAX     5  // how much elements can be removed from bakes list at once
#define MINUTE_TO_MILLIS(m)     ((m) * 60 * 1000)
#define HOUR_TO_MILLIS(h)       ((h) * 60 * 60 * 1000)
#define MAX_ALLOWED_TIME        ( HOUR_TO_MILLIS(99) + MINUTE_TO_MILLIS(59) )   // 100 hours max

typedef enum operationButton {
    BUTTONS_START = 1,
    BUTTONS_PAUSE_STOP,
    BUTTONS_CONTINUE_STOP,
    BUTTONS_MAX_COUNT
} buttonsGroup_t;

// available options
typedef enum optionType {
    OPTION_BUZZER = 0,      //count from 0 (used as index)
    OPTION_OTA,
    OPTION_BAKES_RELOAD,
    OPTION_SAVE,
    OPTION_MAX_COUNT
} optionType_t;

typedef enum valueType {
    OPT_VAL_BOOL = 1,
    OPT_VAL_INT,
    OPT_VAL_TRIGGER,
    OPT_VAL_MAX_COUNT
} valueType_t;

typedef union currentValue {
    bool        bValue;
    int32_t     iValue;
    uint32_t   uiValue;
} currentValue_t;

typedef struct setting {
    char name[32];
    valueType_t valuetype;
    currentValue_t currentValue;
    lv_obj_t * btn;
    void (* optionCallback)( void );
} setting_t;

typedef void (* updateTimeCb)( uint32_t );
typedef void (* updateTempCb)( uint16_t );
typedef void (* operationCb)( void );
typedef void (* bakePickupCb)( uint32_t, bool );
typedef void (* adjustTimeCb)( int32_t );
typedef void (* removeBakesCb)( const uint8_t * );

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
 * time     - new time that will be shown [ms]
 */
void GUI_SetTargetTime( uint32_t time );

/**
 * Set current time on the screen
 * time     - new time that will be shown in millis
 */
void GUI_SetCurrentTime( uint32_t time );

/**
 * Set a callback function that will be called when new time is provided by user
 * func         -   callback function
 */
void GUI_setTimeCallback( updateTimeCb func );

/**
 * Set a callback function that will be called when new temperature is provided by user
 * func         -   callback function
 */
void GUI_setTempCallback( updateTempCb func );

/**
 * Set a callback function that will be called when user click on START button
 * func         -   callback function
 */
void GUI_setStartCallback( operationCb func );

/**
 * Set a callback function that will be called when user click on STOP button
 * func         -   callback function
 */
void GUI_setStopCallback( operationCb func );

/**
 * Set a callback function that will be called when user click on PAUSE button
 * func         -   callback function
 */
void GUI_setPauseCallback( operationCb func );

/**
 * Set a callback function that will be called when user click on bake list
 * func         -   callback function
 */
void GUI_setBakePickupCallback( bakePickupCb func );

/**
 * Set a callback function that will be called when user click on 'adjTime' option
 * func         -   callback function
 */
void GUI_setAdjustTimeCallback( adjustTimeCb func );

/**
 * Set a callback function that will be called when user click on 'Bakes to remove' option
 * func             -   callback function
 */
void GUI_setRemoveBakesFromListCallback( removeBakesCb func );

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
 * Activate/deactivate blinking frame around the whole screen
 * active           -   whether to activate blinking
 */
void GUI_setBlinkScreenFrame( bool active );

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

/**
 * Set time's progress bar fill out
 * progress         - how much of circle should be filled out [0,1%] (ie. 25 = 2,5%)
 */
void GUI_setTimeBar( uint32_t progress );

/**
 * Set temp's progress bar fill out
 * temp             - current temperature in range from 0 to 'targetTemp'
 */
void GUI_setTempBar( int32_t temp );

/**
 * Set bake name on the screen
 * bakeName         - name that will be shown on the screen
 */
void GUI_setBakeName( const char * bakeName );

/**
 * Set power bar fill out with label (ie. "10%")
 * power            - how much bar should be filled out, sets label also
 */
void GUI_setPowerBar( uint32_t power );

/**
 * Set power indicator (on/off)
 * active           - whether power indicator should be glowing
 */
void GUI_setPowerIndicator( bool active );

/**
 * Activate sound icon
 * active           - whether the icon should be glowing
 */
void GUI_setSoundIcon( bool active );

/**
 * Activate WiFi icon
 * active           - whether the icon should be glowing
 */
void GUI_setWiFiIcon( bool active );

/**
 * Populate settings screen with provided options
 * options          - pointer to options data
 * cnt              - number of options
 */
void GUI_optionsPopulate( setting_t options[], uint32_t cnt );

/**
 * Update option according to internal current state
 * option           - pointer to option data
 */
void GUI_updateOption( setting_t &option );

#endif  // _GUI_H
