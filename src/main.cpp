#include <Arduino.h>
#include "gui.h"
#include "heater.h"
#include "myOTA.h"
#include "buzzer.h"
#include "helper.h"
#include "SD.h"
#include "FS.h"

#define SD_CS 34

heater_state heaterState = STATE_IDLE;
heater_state heaterStateRequested = STATE_IDLE;
unsigned long currentTime, next10S, next1S, next10mS, next100mS;
static uint32_t targetHeatingTime;
static uint16_t targetHeatingTemp;
static xTimerHandle xTimer;

String dataMessage;
void writeFile(fs::FS &fs, const char * path, const char * message);

static void updateTime( uint32_t time ) {
  targetHeatingTime = time;
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

  // GUI callbacks
  GUI_setTimeCallback( updateTime );      // time will be updated when changed
  GUI_setTempCallback( updateTemp );      // temp will be updated when changed
  GUI_setStartCallback( heatingStart );   // START heating was clicked
  GUI_setStopCallback( heatingStop );     // STOP heating was clicked
  GUI_setPauseCallback( heatingPause );   // PAUSE heating was clicked

  // test timer feature
  xTimer = xTimerCreate( "myTimer", 10000, pdFALSE, NULL, myTimerCallback );
  xTimerStart( xTimer, 5000 );

  currentTime = next10S = next1S = next10mS = millis();

  // Initialize SD card
  SD.begin( SD_CS );  
  if( !SD.begin( SD_CS ) ) {
    Serial.println( "Card Mount Failed" );
    return;
  }
  uint8_t cardType = SD.cardType();
  if( cardType == CARD_NONE ) {
    Serial.println( "No SD card attached" );
    return;
  }
  // If the data.txt file doesn't exist
  // Create a file on the SD card and write the data labels
  File file = SD.open("/data.txt");
  if(!file) {
    Serial.println("File doens't exist");
    Serial.println("Creating file...");
    writeFile(SD, "/data.txt", "Reading ID, Date, Hour, Temperature \r\n");
  }
  else {
    Serial.println("File already exists");  
  }
  file.close();
}

// Write to the SD card (DON'T MODIFY THIS FUNCTION)
void writeFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if(!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  if(file.print(message)) {
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}

// Append data to the SD card (DON'T MODIFY THIS FUNCTION)
void appendFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if(!file) {
    Serial.println("Failed to open file for appending");
    return;
  }
  if(file.print(message)) {
    Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
  }
  file.close();
}

void logSDCard() {
  dataMessage = String(12) + "," + String(34) + "," + String(56) + "," + String(78) + "\r\n";
  Serial.print( "Save data: " );
  Serial.println( dataMessage );
  appendFile( SD, "/data.txt", dataMessage.c_str() );
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
    GUI_SetCurrentTemp( (uint16_t)HEATER_getCurrentTemperature() );
    uint32_t timeRemaining = HEATER_getTimeRemaining();

    // show time with seconds when time is less than 1h
    if( MINUTES_TO_MS(60) > timeRemaining ) {
      timeRemaining = MM_SS_TO_HH_MM( timeRemaining );
    }

    OTA_LogWrite( timeRemaining );
    // Serial.println(timeRemaining);
    GUI_SetCurrentTime( timeRemaining );
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
    logSDCard();
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

        // hide "Start" button, show "Pause" and "Stop" buttons
        GUI_setOperationButtons( BUTTONS_PAUSE_STOP );
        GUI_setTimeTempChangeAllowed( false );
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
        GUI_setBlinkTimeCurrent( false );   // no required here but just in case

        heaterStateRequested = STATE_IDLE;
        heaterState = STATE_IDLE;
      }
      else if( STATE_PAUSE_REQUESTED == heaterStateRequested ) {
        HEATER_pause();
        GUI_setOperationButtons( BUTTONS_CONTINUE_STOP );
        GUI_setTimeTempChangeAllowed( false );
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
        GUI_setBlinkTimeCurrent( false );    // stop blinking (stop heating)

        heaterStateRequested = STATE_IDLE;
        heaterState = STATE_IDLE;
      }
      else if( STATE_PAUSE_REQUESTED == heaterStateRequested ) {
        HEATER_pause(); // continue processing (same API function for 'unpause')
        GUI_setOperationButtons( BUTTONS_PAUSE_STOP );
        GUI_setTimeTempChangeAllowed( false );
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
