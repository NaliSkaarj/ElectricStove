#include <Arduino.h>
#include "gui.h"
#include "PID.h"
#include "myOTA.h"
#include "buzzer.h"

bool color = true;
unsigned long currentTime, next10S, next1S, next10mS, next100mS;

void myTimerCallback( xTimerHandle pxTimer ) 
{
  GUI_SetTabActive( 1 );
  Serial.println( "Timer callback function executed" );
  BUZZ_Add( 2000, 1000, 500, 5 );
}

void setup() {
  // input = 20;
  Serial.begin( 115200 );

  GUI_Init();
  PID_Init();
  PID_SetPoint( 100 );
  PID_On();
  BUZZ_Init();
  OTA_Setup();

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
    GUI_Handle( 10 );
    next10mS += 10;
    PID_Compute();
  }

  // handle stuff every 100 miliseconds
  if( currentTime >= next100mS ) {
    static uint8_t t = 0;
    GUI_SetTemp( t++ );
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
}
