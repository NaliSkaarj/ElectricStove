#include <WiFi.h>
#include "myOTA.h"
#include <ArduinoOTA.h>
#include "credentials.h"

WiFiServer server( PORT ); // server port to listen on
WiFiClient client;

void OTA_LogWrite( const char *buf ) {
  if ( client && client.connected() ) {
    client.write( buf, strlen(buf) );
  }
}

void OTA_Setup() {
  ArduinoOTA.setHostname( OTA_HOST_NAME );
  Serial.println( "OTA setup" );
  WiFi.mode( WIFI_STA );
  WiFi.begin( wifi_SSID, wifi_PASS );
  while ( WiFi.waitForConnectResult() != WL_CONNECTED ) {
    Serial.println( "Connection Failed! Rebooting..." );
    delay( 5000 );
    ESP.restart();
  }

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

  ArduinoOTA.begin();

  server.begin();
  server.setNoDelay( true );

  Serial.println( "OTA ready!" );
  Serial.print( "IP address: " );
  Serial.println( WiFi.localIP() );
}

void OTA_Handle() {
  ArduinoOTA.handle();

  if ( WiFi.status() == WL_CONNECTED ) {
    if ( server.hasClient() ) {
      if ( !client || !client.connected() ) {
        if ( client ) {
          client.stop();
          // Serial.println( "client.stop()//1" );
        }
        client = server.accept();
        if ( !client ) {
          Serial.println( "available broken" );
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
        // Serial.println( "client.stop()//2" );
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
      Serial.println( "WiFi lost! Client droped." );
    }
  }
}
