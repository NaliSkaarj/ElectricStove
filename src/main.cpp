#include <Arduino.h>
#include "gui.h"
#include "heater.h"
#include "myOTA.h"
#include "buzzer.h"
#include "helper.h"

heater_state heaterState = STATE_IDLE;
heater_state heaterStateRequested = STATE_IDLE;
unsigned long currentTime, next10S, next1S, next10mS, next100mS;
// static uint32_t currentHeatingTime;
static uint32_t targetHeatingTime, targetHeatingTimeOld;
// static float currentHeatingTemp;
static float targetHeatingTemp, targetHeatingTempOld;

static void updateTime( uint32_t time ) {
  targetHeatingTime = time;
  GUI_SetTargetTime( targetHeatingTime );
}

static void updateTemp( float temp ) {
  targetHeatingTemp = temp;
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
  // input = 20;
  Serial.begin( 115200 );

  OTA_Setup();
  BUZZ_Init();
  GUI_Init();
  HEATER_Init();
  HEATER_setCallback( heatingDone );

  // GUI callbacks
  GUI_setTimeCallback( updateTime );      // time will be updated when changed
  GUI_setTempCallback( updateTemp );      // temp will be updated when changed
  GUI_setStartCallback( heatingStart );   // START heating was clicked
  GUI_setStopCallback( heatingStop );    // STOP heating was clicked
  GUI_setPauseCallback( heatingPause );   // PAUSE heating was clicked

  // test timer feature
  xTimerHandle xTimer = xTimerCreate( "myTimer", 10000, pdFALSE, NULL, myTimerCallback );
  xTimerStart( xTimer, 5000 );

  currentTime = next10S = next1S = next10mS = millis();
}

void loop() {
  OTA_Handle();
  currentTime = millis();

  // handle stuff every 10 miliseconds
  if( currentTime >= next10mS ) {
    if( targetHeatingTempOld != targetHeatingTemp ) {
      GUI_SetTargetTemp( targetHeatingTemp );
      targetHeatingTempOld = targetHeatingTemp;
    }
    if( targetHeatingTimeOld != targetHeatingTime ) {
      GUI_SetTargetTime( targetHeatingTime );
      targetHeatingTimeOld = targetHeatingTime;
    }

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
    GUI_SetCurrentTemp( (uint16_t)HEATER_getCurrentTemperature() );
    GUI_SetCurrentTime( HEATER_getTimeRemaining() );
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
  }

  switch( heaterState ) {
    case STATE_IDLE: {
      // check against started heating
      if( STATE_START_REQUESTED == heaterStateRequested ) {
        HEATER_setTime( targetHeatingTime );
        HEATER_setTemperature( (uint16_t)targetHeatingTemp );
        HEATER_start();

        // hide "Start" button, show "Pause" and "Stop" buttons
        GUI_setOperationButtons( BUTTON_PAUSE_STOP );

        heaterStateRequested = STATE_IDLE;
        heaterState = STATE_HEATING;
      }
      else {
        heaterStateRequested = STATE_IDLE;  // in STATE_IDLE only STATE_START_REQUESTED allowed
      }
      break;
    }
    case STATE_HEATING: {
      if( STATE_STOP_REQUESTED == heaterStateRequested ) {
        HEATER_stop();
        GUI_setOperationButtons( BUTTON_START );

        heaterStateRequested = STATE_IDLE;
        heaterState = STATE_IDLE;
      }
      else if( STATE_PAUSE_REQUESTED == heaterStateRequested ) {
        HEATER_pause();
        GUI_setOperationButtons( BUTTON_PAUSE_STOP );

        heaterStateRequested = STATE_IDLE;
        heaterState = STATE_HEATING_PAUSE;
      }
      break;
    }
    case STATE_HEATING_PAUSE: {
      if( STATE_STOP_REQUESTED == heaterStateRequested ) {
        HEATER_stop();
        GUI_setOperationButtons( BUTTON_START );

        heaterStateRequested = STATE_IDLE;
        heaterState = STATE_IDLE;
      }
      else if( STATE_PAUSE_REQUESTED == heaterStateRequested ) {
        HEATER_start();
        GUI_setOperationButtons( BUTTON_PAUSE_STOP );

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
