#include <Arduino.h>
#include "TFT_eSPI.h"
#include "lvgl.h"
#include "PID.h"
#include <WiFi.h>
#include <ArduinoOTA.h>
#include "credentials.h"

#define PORT 23
const char *ssid = wifi_SSID;
const char *password = wifi_PASS;

WiFiServer server( PORT ); // server port to listen on
WiFiClient client;

// extern double input, output;   // testing PID module

uint16_t x, y;
bool color = true;
unsigned long currentTime, next1S, next10mS;
lv_obj_t * slider_label;
lv_obj_t * tabview;

TFT_eSPI tft = TFT_eSPI();
static lv_color_t buf[LV_HOR_RES_MAX * LV_VER_RES_MAX / 10]; // Declare a buffer for 1/10 screen size

void logWrite( const char *buf ) {
  if ( client && client.connected() ) {
    client.write( buf, strlen(buf) );
  }
}

void OTASetup() //do obslugi flashowania przez wifi, musi byc wywolane w setup
{
  Serial.println( "OTA setup" );
  WiFi.mode( WIFI_STA );
  WiFi.begin( ssid, password );
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
      if (ArduinoOTA.getCommand() == U_FLASH) {
        type = "sketch";
      } else {  // U_SPIFFS
        type = "filesystem";
      }

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) {
        Serial.println("Auth Failed");
      } else if (error == OTA_BEGIN_ERROR) {
        Serial.println("Begin Failed");
      } else if (error == OTA_CONNECT_ERROR) {
        Serial.println("Connect Failed");
      } else if (error == OTA_RECEIVE_ERROR) {
        Serial.println("Receive Failed");
      } else if (error == OTA_END_ERROR) {
        Serial.println("End Failed");
      }
    });

  ArduinoOTA.begin();

  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

/* Display flushing */
void customDisplayFlush( lv_display_t * disp, const lv_area_t * area, uint8_t * color_p )
{
  uint32_t w = ( area->x2 - area->x1 + 1 );
  uint32_t h = ( area->y2 - area->y1 + 1 );

  tft.startWrite();
  tft.setAddrWindow( area->x1, area->y1, w, h );
  tft.myPushColors( color_p, w * h * 3, false );
  tft.endWrite();

  lv_disp_flush_ready( disp );
}

void customTouchpadRead( lv_indev_t * indev_driver, lv_indev_data_t * data )
{
  uint16_t touchX, touchY;

  bool touched = tft.getTouch( &touchX, &touchY );

  if( touched ) {
    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = touchX;
    data->point.y = touchY;
    // Serial.printf( "x: %i     ", touchX );
    // Serial.printf( "y: %i \n", touchY );
  }
  else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

void myTimerCallback( xTimerHandle pxTimer ) 
{
  lv_tabview_set_active( tabview, 1, LV_ANIM_OFF );
  Serial.println( "Timer callback function executed" );
}

void setup() {
  // input = 20;
  Serial.begin( 115200 );

  uint16_t calData[5] = { 265, 3677, 261, 3552, 1 };
  tft.init();
  tft.setRotation( 3 );
  tft.setSwapBytes( true );
  tft.setTouch( calData );

  lv_init();

  // init DISPLAY
  lv_display_t *display = lv_display_create( LV_HOR_RES_MAX, LV_VER_RES_MAX );
  lv_display_set_buffers( display, buf, NULL, sizeof(buf), LV_DISPLAY_RENDER_MODE_PARTIAL );  // Initialize the display buffer.
  lv_display_set_flush_cb( display, customDisplayFlush );

  // init TOUCHSCREEN
  lv_indev_t * indev = lv_indev_create();               // Create an input device
  lv_indev_set_type( indev, LV_INDEV_TYPE_POINTER );    // Touch pad is a pointer-like device
  lv_indev_set_read_cb( indev, customTouchpadRead );    // Set your driver function
  lv_indev_enable( indev, true );

  /*Create a Tab view object*/
  tabview = lv_tabview_create( lv_scr_act() );
  lv_tabview_set_tab_bar_position( tabview, LV_DIR_RIGHT );
  lv_tabview_set_tab_bar_size( tabview, 80 );

  lv_obj_set_style_bg_color( tabview, lv_palette_lighten(LV_PALETTE_RED, 2), 0 );

  lv_obj_t * tab_buttons = lv_tabview_get_tab_bar( tabview );
  lv_obj_set_style_bg_color( tab_buttons, lv_palette_darken(LV_PALETTE_GREY, 3), 0 );
  lv_obj_set_style_text_color( tab_buttons, lv_palette_lighten(LV_PALETTE_GREY, 5), 0 );
  lv_obj_set_style_border_side( tab_buttons, LV_BORDER_SIDE_RIGHT, LV_PART_ITEMS | LV_STATE_CHECKED );

  static lv_style_t styleTab;
  lv_style_init( &styleTab );
  lv_style_set_text_decor( &styleTab, LV_TEXT_DECOR_NONE );
  lv_style_set_text_font( &styleTab, &lv_font_montserrat_32 );
  lv_obj_add_style( tabview, &styleTab, 0 );
  /*Add 3 tabs (the tabs are page (lv_page) and can be scrolled*/
  lv_obj_t * tab1 = lv_tabview_add_tab( tabview, LV_SYMBOL_HOME );
  lv_obj_t * tab2 = lv_tabview_add_tab( tabview, LV_SYMBOL_LIST );
  lv_obj_t * tab3 = lv_tabview_add_tab( tabview, LV_SYMBOL_SETTINGS );

  lv_obj_set_style_bg_color( tab1, {0xff, 0x00, 0x00}, 0 ); // red
  lv_obj_set_style_bg_opa( tab1, LV_OPA_COVER, 0 );
  lv_obj_set_style_bg_color( tab2, {0x00, 0xff, 0x00}, 0 ); // green
  lv_obj_set_style_bg_opa( tab2, LV_OPA_COVER, 0 );
  lv_obj_set_style_bg_color( tab3, {0x00, 0x00, 0xff}, 0 ); // blue
  lv_obj_set_style_bg_opa( tab3, LV_OPA_COVER, 0 );

  /*Add content to the tabs*/
  lv_obj_t * label = lv_label_create( tab1 );
  lv_label_set_text( label, "First tab" );

  label = lv_label_create( tab2 );
  lv_label_set_text( label, "Second tab (0F0 green)" );

  label = lv_label_create( tab3 );
  lv_label_set_text( label, "Third tab (00F blue)" );

  static lv_style_t style2;
  lv_style_init( &style2 );
  lv_style_set_text_decor( &style2, LV_TEXT_DECOR_NONE );
  lv_style_set_text_font( &style2, &lv_font_montserrat_32 );
  lv_obj_add_style( tab1, &style2, 0 );
  lv_obj_add_style( tab2, &style2, 0 );
  lv_obj_add_style( tab3, &style2, 0 );

  lv_obj_remove_flag( lv_tabview_get_content(tabview), LV_OBJ_FLAG_SCROLLABLE );

  // test some features
  xTimerHandle xTimer = xTimerCreate( "myTimer", 2000, pdFALSE, NULL, myTimerCallback );
  xTimerStart( xTimer, 1000 );


  // init PID
  PID_Init();
  PID_SetPoint( 100 );
  PID_On();

  currentTime = next1S = next10mS = millis();

  // OTA
  ArduinoOTA.setHostname( "ElectricStove" );
  OTASetup();
  Serial.println( "Server begin" );
  server.begin();
  server.setNoDelay( true );

  Serial.println( "Setup finished." );
}

void loop() {
  // ArduinoOTA.handle();
  currentTime = millis();

  // handle stuff every 10 miliseconds
  if( currentTime >= next10mS ) {
    lv_timer_handler();
    lv_tick_inc( 10 );
    next10mS += 10;
    PID_Compute();
    // ArduinoOTA.handle();
  }

  // handle stuff every 1 second
  if( currentTime >= next1S ) {
    Serial.print( "*" );
    logWrite( "#" );
    next1S += 1000;

    // for testing PID module
    // if( input < 100 ) {
    //   input++;
    // }
    // Serial.print( input );
    // Serial.print( ": " );
    // Serial.println( output );
    ArduinoOTA.handle();
  }

  if (WiFi.status() == WL_CONNECTED) {
    //check if there are any new clients
    if (server.hasClient()) {
      //find free/disconnected spot
      if (!client || !client.connected()) {
        if (client) {
          client.stop();
          Serial.println("111");
        }
        client = server.accept();
        if (!client) {
          Serial.println("available broken");
        }
        Serial.print("New client: ");
        Serial.println(client.remoteIP());
      }
    }
    //check clients for data
    if (client && client.connected()) {
      if (client.available()) {
        //get data from the telnet client and push it to the UART
        while (client.available()) {
          Serial.write(client.read());
        }
      }
    } else {
      if (client) {
        client.stop();
        Serial.println("aaa");
      }
    }
    //check UART for data
    if (Serial.available()) {
      size_t len = Serial.available();
      uint8_t sbuf[len];
      Serial.readBytes(sbuf, len);
      //push UART data to all connected telnet clients
      if (client && client.connected()) {
        client.write(sbuf, len);
        delay(1);
      }
    }
  } else {
    Serial.println("WiFi not connected!");
    if (client) {
      client.stop();
      Serial.println("bbb");
    }
    delay(1000);
  }
}
