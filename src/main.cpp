#include <Arduino.h>
#include "gui.h"
#include "heater.h"
#include "myOTA.h"
#include "buzzer.h"
#include "helper.h"
#include "config.h"

heater_state heaterState = STATE_IDLE;
heater_state heaterStateRequested = STATE_IDLE;
unsigned long currentTime, next10S, next1S, next10mS, next100mS;
static uint32_t targetHeatingTime;    // in miliseconds
static uint16_t targetHeatingTemp;
static xTimerHandle xTimer;
static bakeName *bakeNames = NULL;
static uint32_t bakeCount;
static uint32_t bakeIdx;
// populate GUI options
static setting_t settings[] = {     // preserve order according to optionType enum
  { "Buzzer activation", OPT_VAL_BOOL, 1, NULL },
  { "OTA activation", OPT_VAL_BOOL, 1, NULL },
  { "Reload Bakes file", OPT_VAL_TRIGGER, 0, NULL },
};


static void updateTime( uint32_t time ) {
  targetHeatingTime = time;

  if( MAX_ALLOWED_TIME < targetHeatingTime ) {
    targetHeatingTime = MAX_ALLOWED_TIME;
  }

  GUI_SetTargetTime( targetHeatingTime );
}

static void updateTemp( uint16_t temp ) {
  targetHeatingTemp = temp;

  if( MAX_ALLOWED_TEMP < targetHeatingTemp ) {
    targetHeatingTemp = MAX_ALLOWED_TEMP;
  }
  if( MIN_ALLOWED_TEMP > targetHeatingTemp ) {
    targetHeatingTemp = MIN_ALLOWED_TEMP;
  }

  GUI_SetTargetTemp( targetHeatingTemp );
}

static void heatingStart() {
  heaterStateRequested = STATE_START_REQUESTED;
}

static void heatingStop() {
  heaterStateRequested = STATE_STOP_REQUESTED;
}

static void heatingPause() {
  heaterStateRequested = STATE_PAUSE_REQUESTED;
}

static void bakePickup( uint32_t idx, bool longPress ) {
  if( STATE_IDLE == heaterState ) {
    bakeIdx = idx;

    targetHeatingTemp = CONF_getBakeTemp( bakeIdx );
    targetHeatingTime = SECONDS_TO_MILISECONDS( CONF_getBakeTime( bakeIdx ) );
    GUI_SetTargetTemp( targetHeatingTemp );
    GUI_SetTargetTime( targetHeatingTime );
    GUI_setBakeName( CONF_getBakeName( bakeIdx ) );

    if( longPress ) {
      heaterStateRequested = STATE_START_REQUESTED;
    }

    Serial.printf( "Bake pickup[%d]:\"%s\"; Time:%d[ms], Temp:%d[*C]\n", bakeIdx + 1, CONF_getBakeName( bakeIdx ), targetHeatingTime, targetHeatingTemp );
  } else {
    BUZZ_Add( 0, 80, 100, 3 );
  }

  GUI_SetTabActive( TAB_MAIN );
}

static void buzzerActivation() {
  if( true == settings[ OPTION_BUZZER ].currentValue.bValue ) {
    Serial.println( "BUZZER deactivated" );
    settings[ OPTION_BUZZER ].currentValue.bValue = false;
  } else {
    Serial.println( "BUZZER activated" );
    settings[ OPTION_BUZZER ].currentValue.bValue = true;
  }

  BUZZ_Activate( settings[ OPTION_BUZZER ].currentValue.bValue );
  GUI_setSoundIcon( settings[ OPTION_BUZZER ].currentValue.bValue );
  GUI_updateOption( settings[ OPTION_BUZZER ] );
  // GUI_SetTabActive( 0 );
}

static void otaActivation() {
  if( true == settings[ OPTION_OTA ].currentValue.bValue ) {
    OTA_Off();
    settings[ OPTION_OTA ].currentValue.bValue = false;
  } else {
    OTA_On();
    settings[ OPTION_OTA ].currentValue.bValue = true;
  }

  GUI_setWiFiIcon( settings[ OPTION_OTA ].currentValue.bValue );
  GUI_updateOption( settings[ OPTION_OTA ] );
  // GUI_SetTabActive( 0 );
}

static void bakesReload() {
  Serial.println( "Reloading Bakes file..." );
  CONF_reloadBakeFile();
  free( bakeNames );
  CONF_getBakeNames( &bakeNames, &bakeCount );
  GUI_populateBakeListNames( (char *)bakeNames, BAKE_NAME_LENGTH, bakeCount );
  GUI_SetTabActive( 1 );
}

static void adjustTime( int32_t time ) {
  int32_t newTime = targetHeatingTime + MINUTE_TO_MILLIS( time );
  Serial.printf( "Adjust Time: %d[min]\n", time );

  if( 0 < newTime && MAX_ALLOWED_TIME > newTime ) {
    targetHeatingTime = newTime;
    GUI_SetTargetTime( targetHeatingTime );

    if( STATE_HEATING == heaterState
    || STATE_HEATING_PAUSE == heaterState ) {
      HEATER_setTime( targetHeatingTime );
    }
  }
}

static void myTimerCallback( xTimerHandle pxTimer ) 
{
  // GUI_SetTabActive( 1 );
  Serial.println( "Timer callback function executed" );
  BUZZ_Add( 0, 100, 100, 5 );
}

static void heatingDone( void ) {
  BUZZ_Add( 0, 1000, 200, 5 );
  ///TODO: handle case when current phase of heating process is finished and next phase can be performed
  OTA_LogWrite( "Heating done!\n" );
  heaterStateRequested = STATE_STOP_REQUESTED;
}

size_t getArduinoLoopTaskStackSize(void) {
  return (10 * 1024);
}

void setup() {
  Serial.begin( 115200 );

  OTA_Setup();
  BUZZ_Init();
  GUI_Init();
  HEATER_Init( GUI_getSPIinstance() );
  HEATER_setCallback( heatingDone );
  CONF_Init( GUI_getSPIinstance() );

  // GUI callbacks
  GUI_setTimeCallback( updateTime );      // time will be updated when changed
  GUI_setTempCallback( updateTemp );      // temp will be updated when changed
  GUI_setStartCallback( heatingStart );   // START heating was clicked
  GUI_setStopCallback( heatingStop );     // STOP heating was clicked
  GUI_setPauseCallback( heatingPause );   // PAUSE heating was clicked
  GUI_setBakePickupCallback( bakePickup );
  GUI_setAdjustTimeCallback( adjustTime );

  CONF_getBakeNames( &bakeNames, &bakeCount );
  GUI_populateBakeListNames( (char *)bakeNames, BAKE_NAME_LENGTH, bakeCount );
  // setup GUI options callbacks
  settings[ OPTION_BUZZER ].optionCallback = buzzerActivation;
  settings[ OPTION_OTA ].optionCallback = otaActivation;
  settings[ OPTION_BAKES_RELOAD ].optionCallback = bakesReload;
  GUI_optionsPopulate( settings, sizeof(settings)/sizeof(setting_t) );

  // test timer feature
  xTimer = xTimerCreate( "myTimer", 10000, pdFALSE, NULL, myTimerCallback );
  xTimerStart( xTimer, 5000 );

  currentTime = next10S = next1S = next10mS = millis();
}

void loop() {
  OTA_Handle();
  currentTime = millis();

  // handle stuff every 10 miliseconds
  if( currentTime >= next10mS ) {
    GUI_Handle( 10 );
    next10mS += 10;

    // trick (GUI_Handle() can take over 200ms)
    unsigned long currentTimeTmp = millis();
    if( currentTimeTmp >= next10mS ) {
      next10mS = currentTimeTmp + 10; // if shifted next10ms is too little, move it forward against current time
    }
  }

  // handle stuff every 100 miliseconds
  if( currentTime >= next100mS ) {
    float currentTemp = HEATER_getCurrentTemperature();
    uint32_t timeRemaining = HEATER_getTimeRemaining();
    uint8_t power = HEATER_getCurrentPower();

    if( 0 < targetHeatingTime ) {
      uint32_t barTime = 1000 - (uint32_t)( (float)timeRemaining * 1000 / (float)targetHeatingTime );
      GUI_setTimeBar( barTime );
    } else {
      GUI_setTimeBar( 0 );
    }

    if( 0 < targetHeatingTemp && 0.0f < currentTemp ) {
      int32_t barTemp = (int32_t)( currentTemp * 100 / (float)targetHeatingTemp );
      GUI_setTempBar( barTemp );
    } else {
      GUI_setTempBar( 20 );   // room temp. by default
    }

    // show time with seconds when time is less than 1h
    if( MINUTES_TO_MS(60) > timeRemaining ) {
      timeRemaining = MM_SS_TO_HH_MM( timeRemaining );
    }

    OTA_LogWrite( timeRemaining );
    GUI_SetCurrentTemp( (uint16_t)currentTemp );
    GUI_SetCurrentTime( timeRemaining );
    GUI_setPowerBar( power );
    GUI_setPowerIndicator( HEATER_isHeating() );
    next100mS += 100;
  }

  // handle stuff every 1 second
  if( currentTime >= next1S ) {
    Serial.print( "*" );
    OTA_LogWrite( "?" );
    next1S += 1000;
  }

  // handle stuff every 10 second
  if( currentTime >= next10S ) {
    next10S += 10000;
    BUZZ_Add( 100 );
    // Serial.println( ((String)CONF_getOption( BUZZER_MENU )).c_str() );
    // Serial.println( ((String)CONF_getOption( BUZZER_HEATING )).c_str() );
    // Serial.println( ((String)CONF_getOption( OTA_ACTIVE )).c_str() );
    // for( int x=0; x<bakeCount; x++) {
    //   Serial.printf( "bakeNames[%d]: %s\n", x, bakeNames+x );
    // }
  }

  switch( heaterState ) {
    case STATE_IDLE: {
      // check against started heating
      if( STATE_START_REQUESTED == heaterStateRequested ) { // in STATE_IDLE only STATE_START_REQUESTED allowed
        if( MAX_ALLOWED_TIME < targetHeatingTime ) {
          targetHeatingTime = MAX_ALLOWED_TIME;
        }
        if( MAX_ALLOWED_TEMP < targetHeatingTemp ) {
          targetHeatingTemp = MAX_ALLOWED_TEMP;
        }
        if( MIN_ALLOWED_TEMP > targetHeatingTemp ) {
          targetHeatingTemp = MIN_ALLOWED_TEMP;
        }

        GUI_SetTargetTime( targetHeatingTime );
        GUI_SetTargetTemp( targetHeatingTemp );

        HEATER_setTime( targetHeatingTime );
        HEATER_setTemperature( (uint16_t)targetHeatingTemp );
        HEATER_start();

        BUZZ_Add( 400 );

        // hide "Start" button, show "Pause" and "Stop" buttons
        GUI_setOperationButtons( BUTTONS_PAUSE_STOP );
        GUI_setTimeTempChangeAllowed( false );
        GUI_setBlinkScreenFrame( true );
        GUI_setBlinkTimeCurrent( false );    // no required here but just in case

        heaterStateRequested = STATE_IDLE;
        heaterState = STATE_HEATING;
      }
      break;
    }
    case STATE_HEATING: {
      if( STATE_STOP_REQUESTED == heaterStateRequested ) {
        HEATER_stop();
        GUI_setOperationButtons( BUTTONS_START );
        GUI_setTimeTempChangeAllowed( true );
        GUI_setBlinkScreenFrame( false );
        GUI_setBlinkTimeCurrent( false );   // no required here but just in case

        heaterStateRequested = STATE_IDLE;
        heaterState = STATE_IDLE;
      }
      else if( STATE_PAUSE_REQUESTED == heaterStateRequested ) {
        HEATER_pause();
        GUI_setOperationButtons( BUTTONS_CONTINUE_STOP );
        GUI_setTimeTempChangeAllowed( false );
        GUI_setBlinkScreenFrame( false );
        GUI_setBlinkTimeCurrent( true );   // blinking 'TimeCurrent' indicate PAUSE active

        heaterStateRequested = STATE_IDLE;
        heaterState = STATE_HEATING_PAUSE;
      }
      break;
    }
    case STATE_HEATING_PAUSE: {
      if( STATE_STOP_REQUESTED == heaterStateRequested ) {
        HEATER_stop();
        GUI_setOperationButtons( BUTTONS_START );
        GUI_setTimeTempChangeAllowed( true );
        GUI_setBlinkScreenFrame( false );
        GUI_setBlinkTimeCurrent( false );    // stop blinking (stop heating)

        heaterStateRequested = STATE_IDLE;
        heaterState = STATE_IDLE;
      }
      else if( STATE_PAUSE_REQUESTED == heaterStateRequested ) {
        HEATER_pause(); // continue processing (same API function for 'unpause')
        GUI_setOperationButtons( BUTTONS_PAUSE_STOP );
        GUI_setTimeTempChangeAllowed( false );
        GUI_setBlinkScreenFrame( true );
        GUI_setBlinkTimeCurrent( false );    // stop blinking (continue heating)

        heaterStateRequested = STATE_IDLE;
        heaterState = STATE_HEATING;
      }
      break;
    }
    default: {
      break;
    }
  }
}
