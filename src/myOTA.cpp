#include <WiFi.h>
#include "myOTA.h"
#include <ArduinoOTA.h>
#include "credentials.h"
#include "config.h"

static TaskHandle_t       taskHandle = NULL;
static StaticTask_t       taskTCB;
static StackType_t        taskStack[ OTA_STACK_SIZE ];

bool initialized = false;
bool otaOnRequested = false;
bool otaOffRequested = false;
static otaActiveCb otaActiveCB = NULL;

WiFiServer server( PORT ); // server port to listen on
WiFiClient client;

static void commandHandle( uint8_t*, int );
static void showCurveList();
static void showOneCurve( int );
static void addCurveToList();

static void WiFiEvent( WiFiEvent_t event, WiFiEventInfo_t info ) {
  if ( event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED ) {
      Serial.print( "Wi-Fi disconnected! Reason: " );
      Serial.println( WiFi.disconnectReasonName( (wifi_err_reason_t)info.wifi_sta_disconnected.reason ) );
  }
}

static bool otaOn() {
  if( WL_CONNECTED == WiFi.status() ) {
    Serial.println( "connected" );
    ArduinoOTA.begin();
    server.begin();
    server.setNoDelay( true );
    initialized = true;
  
    Serial.println( "OTA ready!" );
    Serial.print( "IP address: " );
    Serial.println( WiFi.localIP() );
  } else {
    Serial.print( "." );
    initialized = false;
  }

  return initialized;
}

static void otaOff() {
  if ( client ) {
    client.stop();
  }

  server.end();
  ArduinoOTA.end();
  WiFi.disconnect();
  Serial.println( "WiFi disconnected. OTA disabled." );
  initialized = false;
}

static void otaHandle() {
  if( false == initialized ) {
    return;
  }

  ArduinoOTA.handle();

  if ( WiFi.status() == WL_CONNECTED ) {
    if ( server.hasClient() ) {
      if ( !client || !client.connected() ) {
        if ( client ) {
          client.stop();
        }
        client = server.accept();
        if ( !client ) {
          Serial.println( "client available broken" );
        }
        Serial.print( "New client: " );
        Serial.println( client.remoteIP() );
      }
    }
    //check clients for data
    if ( client && client.connected() ) {
      String clientBuffer = "";

      while( client.available() ) {
        char c = client.read();
        Serial.write( c );

        if( c == '\n' ) {
          clientBuffer.trim();      // usuń spacje i CR/LF
          if( clientBuffer.length() > 0 ) {
            commandHandle( (uint8_t*)clientBuffer.c_str(), clientBuffer.length() );
          }
          clientBuffer = "";      //reset bufora
        } else {
          clientBuffer += c;      //dopisujemy znak
        }
      }
    } else {
      if ( client ) {
        client.stop();
      }
    }
    //check UART for data
    if( Serial.available() ) {
      size_t len = Serial.available();
      uint8_t sbuf[len];
      Serial.readBytes( sbuf, len );
      //push UART data to telnet client
      if ( client && client.connected() ) {
        client.write( sbuf, len );
      }
    }
  } else {
    Serial.println( "WiFi connection lost." );

    if ( client ) {
      client.stop();
      Serial.println( "OTA(handle): WiFi lost! Client dropped" );
    }

    server.end();
    ArduinoOTA.end();
    WiFi.disconnect();

    initialized = false;
    if( NULL != otaActiveCB ) {
      otaActiveCB( initialized );
    }
  }
}

static void commandHandle( uint8_t* buf, int len ){
  // convert buffer to string (assuming that's an ASCII)
  String input = "";
  for( int i = 0; i < len; i++ ) {
    if( buf[i] == '\r' || buf[i] == '\n' ) break; // ignore enter
    input += (char)buf[i];
  }

  input.toLowerCase(); // case-insensitive commands

  // divide to command & argument
  int spaceIndex = input.indexOf(' ');
  String cmd = (spaceIndex == -1) ? input : input.substring(0, spaceIndex);
  String arg = (spaceIndex == -1) ? ""    : input.substring(spaceIndex + 1);

  if( cmd == "help" ) {
    client.println( "Dostępne komendy: help, list, show <index>" );
  } else if( cmd == "list" ) {
    showCurveList();
  } else if( cmd == "show" ) {
    showOneCurve( arg.toInt() );
  } else {
    client.print( "Nieznana komenda: " );
    client.write( cmd.c_str(), cmd.length() );
    client.println();
  }
}

static void showCurveList() {
  bakeName *bakeNames = NULL;
  uint32_t bakeCount;
  char buffer[ BAKE_NAME_LENGTH+5 ];  // additional 5 bytes for 3 digits number and 2 static chars ": "

  CONF_getBakeNames( &bakeNames, &bakeCount );
  client.println( "Lista krzywych:" );

  for( int x=0; x<bakeCount; x++ ) {
    snprintf( buffer, sizeof(buffer), "%d: %s", (x+1), bakeNames[x] );
    client.println( buffer );
  }
  free( bakeNames );
}

static void showOneCurve( int idx ) {
  if( idx <= 0 ) {
    client.println( "Wrong index" );
    return;
  }

  char * bakeName = CONF_getBakeName( idx-1 );
  if( 0 == bakeName ) {
    client.println( "Wrong index" );
    return;
  }

  for( int i = 0; i < BAKE_NAME_LENGTH; i++ ) {
    if( bakeName[i] == '\0' ) break;
    client.write( bakeName[i] );
  }
  client.println( ":" );

  char * data = CONF_getBakeSerializedData( idx-1 );
  client.println( data );

  free( data );
}

static void addCurveToList() {
  ;
}

static void vTaskOTA( void * pvParameters ) {
  static bool connecting1stStep = true;
  static uint32_t tryAgain = 100;     // retrying for time: tryAgain * 100ms

  while( 1 ) {
    if( otaOnRequested && !otaOffRequested ) {
      if( connecting1stStep ) {
        Serial.print( "WiFi connecting" );
        WiFi.mode( WIFI_STA );
        WiFi.onEvent( WiFiEvent );
        WiFi.begin( wifi_SSID, wifi_PASS );
        connecting1stStep = false;
      }
      if( otaOn() ) {
        otaOnRequested = false;
        connecting1stStep = true; // reset for next connection
        tryAgain = 50;            // reset for next connection
        if( NULL != otaActiveCB ) {
          otaActiveCB( initialized );
        }
      } else {
        if( 0 == tryAgain ) {
          otaOnRequested = false;
          connecting1stStep = true; // reset for next connection
          tryAgain = 50;            // reset for next connection
          Serial.println( "WiFi connection failed." );
          otaOff();                 // explicitly disconnect wifi
          if( NULL != otaActiveCB ) {
            otaActiveCB( initialized );
          }
        }
        tryAgain--;
      }
    }
    if( otaOffRequested ) {
      if( otaOnRequested ) {
        otaOnRequested = false;     // handle otaOff in next cycle
      } else {
        otaOff();
        otaOffRequested = false;
        connecting1stStep = true;   // reset for next connection
        tryAgain = 50;              // reset for next connection
        if( NULL != otaActiveCB ) {
          otaActiveCB( initialized );
        }
      }
    }

    otaHandle();

    vTaskDelay( 100 / portTICK_PERIOD_MS );
  }
}

void OTA_Init() {
  // Port defaults to 3232
  // ArduinoOTA.setPort(3232);
 
  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA
    .onStart([]() {
      String type;
      if ( ArduinoOTA.getCommand() == U_FLASH ) {
        type = "sketch";
      } else {  // U_SPIFFS
        type = "filesystem";
      }

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println( "Start updating " + type );
    })
    .onEnd([]() {
      Serial.println( "\nEnd" );
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf( "Progress: %u%%\r", (progress / (total / 100)) );
    })
    .onError([](ota_error_t error) {
      Serial.printf( "Error[%u]: ", error );
      if ( error == OTA_AUTH_ERROR ) {
        Serial.println( "Auth Failed" );
      } else if ( error == OTA_BEGIN_ERROR ) {
        Serial.println( "Begin Failed" );
      } else if ( error == OTA_CONNECT_ERROR ) {
        Serial.println( "Connect Failed" );
      } else if ( error == OTA_RECEIVE_ERROR ) {
        Serial.println( "Receive Failed" );
      } else if ( error == OTA_END_ERROR ) {
        Serial.println( "End Failed" );
      }
    });

    ArduinoOTA.setHostname( OTA_HOST_NAME );

    taskHandle = xTaskCreateStaticPinnedToCore( vTaskOTA, "OTA", OTA_STACK_SIZE, NULL, OTA_TASK_PRIORITY, taskStack, &taskTCB, 0 );
    assert( taskHandle );
}

void OTA_Activate( bool activate ) {
  if( activate ) {
    otaOnRequested = true;
  } else {
    otaOffRequested = true;
  }
}

void OTA_setOtaActiveCallback( otaActiveCb func ) {
  if( NULL != func ) {
    otaActiveCB = func;
  }
}

void OTA_LogWrite( const char *buf ) {
  if( false == initialized ) {
    return;
  }

  if ( client && client.connected() ) {
    client.write( buf, strlen(buf) );
  }
}

void OTA_LogWrite( const int x ) {
  if( false == initialized ) {
    return;
  }

  OTA_LogWrite( ((String)x).c_str() );
}
