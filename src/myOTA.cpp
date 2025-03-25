#include <WiFi.h>
#include "myOTA.h"
#include <ArduinoOTA.h>
#include "credentials.h"

static TaskHandle_t       taskHandle = NULL;
static StaticTask_t       taskTCB;
static StackType_t        taskStack[ OTA_STACK_SIZE ];

bool initialized = false;
bool otaOnRequested = false;
bool otaOffRequested = false;
static otaActiveCb otaActiveCB = NULL;

WiFiServer server( PORT ); // server port to listen on
WiFiClient client;

static void otaOn() {
  uint8_t tryAgain = 3;
  Serial.println( "OTA activating..." );
  WiFi.mode( WIFI_STA );
  WiFi.begin( wifi_SSID, wifi_PASS );
  initialized = false;
  while ( WiFi.waitForConnectResult() != WL_CONNECTED ) {
    Serial.println( "WiFi connection Failed! Retrying..." );
    if( 0 == tryAgain ) { 
      Serial.println( "OTA(On): Couldn't connect to WiFi." );
      return;   // no WiFi == no OTA
    }
    tryAgain--;
    vTaskDelay( 3000 / portTICK_PERIOD_MS );
  }

  initialized = true;

  ArduinoOTA.begin();

  server.begin();
  server.setNoDelay( true );

  Serial.println( "OTA ready!" );
  Serial.print( "IP address: " );
  Serial.println( WiFi.localIP() );
}

static void otaOff() {
  if ( client ) {
    client.stop();
  }

  server.end();
  ArduinoOTA.end();
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
      if ( client.available() ) {
        //get data from the telnet client and push it to the UART
        while ( client.available() ) {
          Serial.write( client.read() );
        }
      }
    } else {
      if ( client ) {
        client.stop();
      }
    }
    //check UART for data
    if ( Serial.available() ) {
      size_t len = Serial.available();
      uint8_t sbuf[len];
      Serial.readBytes( sbuf, len );
      //push UART data to telnet client
      if ( client && client.connected() ) {
        client.write( sbuf, len );
      }
    }
  } else {
    if ( client ) {
      client.stop();
      Serial.println( "OTA(handle): WiFi lost! Client droped" );
    }

    server.end();
    ArduinoOTA.end();
    Serial.println( "WiFi disconnected. OTA disabled." );

    initialized = false;
    if( NULL != otaActiveCB ) {
      otaActiveCB( initialized );
    }
  }
}

static void vTaskOTA( void * pvParameters ) {
  while( 1 ) {
    if( otaOnRequested ) {
      otaOn();
      otaOnRequested = false;
      if( NULL != otaActiveCB ) {
        otaActiveCB( initialized );
      }
    }
    if( otaOffRequested ) {
      otaOff();
      otaOffRequested = false;
      if( NULL != otaActiveCB ) {
        otaActiveCB( initialized );
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
