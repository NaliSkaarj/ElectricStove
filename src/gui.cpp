#include "gui.h"
#include "TFT_eSPI.h"
#include "lvgl.h"
#include "buzzer.h"
#include "myOTA.h"

typedef enum rollerType { ROLLER_TIME = 1, ROLLER_TEMP } roller_t;

static lv_obj_t * tabView;    // main container for 3 tabs
static lv_style_t styleTabs;  // has impact on tabs icons size
static lv_obj_t * tabHome;    // the widget where the content of the tab HOME can be created
static lv_obj_t * tabList;    // the widget where the content of the tab LIST can be created
static lv_obj_t * tabOptions; // the widget where the content of the tab OPTIONS can be created
static lv_obj_t * heatingIndicator1;
static lv_obj_t * heatingIndicator2;
static lv_obj_t * heatingIndicator3;
static lv_obj_t * heatingIndicator4;
static bool       touchEvent = false;
static lv_obj_t * labelTargetTempVal;
static lv_obj_t * labelTargetTimeVal;
static lv_obj_t * labelCurrentTempVal;
static lv_obj_t * labelCurrentTimeVal;
static lv_obj_t * containerRoller;
static lv_obj_t * containerButtons;
static lv_obj_t * btnTime;
static lv_obj_t * btnTemp;
static lv_obj_t * roller1;
static lv_obj_t * roller2;
static lv_obj_t * roller3;
static lv_obj_t * roller4;
static lv_obj_t * bakeList;
static updateTimeCb timeChangedCB = NULL;
static updateTempCb tempChangedCB = NULL;
static operationCb heatingStartCB = NULL;
static operationCb heatingStopCB = NULL;
static operationCb heatingPauseCB = NULL;
static bakePickupCb bakePickupCB = NULL;
static buttonsGroup_t buttonsGroup;
static uint16_t rollerTemp;
static uint32_t rollerTime;
static lv_timer_t * timer_blinkTimeCurrent;
static lv_timer_t * timer_blinkScreenFrame;
static lv_timer_t * timer_setDefaultTab;

TFT_eSPI tft = TFT_eSPI();
static lv_color_t buf[LV_HOR_RES_MAX * LV_VER_RES_MAX / 10]; // Declare a buffer for 1/10 screen size

static void customDisplayFlush( lv_display_t * disp, const lv_area_t * area, uint8_t * color_p );
static void customTouchpadRead( lv_indev_t * indev_driver, lv_indev_data_t * data );
static void tabEventCb( lv_event_t * event );
static void touchEventCb( lv_event_t * event );
static void timeEventCb( lv_event_t * event );
static void tempEventCb( lv_event_t * event );
static void btnOkEventCb( lv_event_t * event );
static void btnCancelEventCb( lv_event_t * event );
static void btnStartEventCb( lv_event_t * event );
static void btnStopEventCb( lv_event_t * event );
static void btnPauseEventCb( lv_event_t * event );
static void btnBakeSelectEventCb( lv_event_t * event );
static void rollerCreate( roller_t rType );
static void createOperatingButtons();
static void setContentHome();
static void setContentList( char *nameList, uint32_t nameLength, uint32_t nameCount );
static void setContentOptions();
static void setScreenMain();
static void blinkTimeCurrent( lv_timer_t * timer );
static void blinkScreenFrame( lv_timer_t * timer );
static void setDefaultTab( lv_timer_t * timer );

/* Display flushing */
static void customDisplayFlush( lv_display_t * disp, const lv_area_t * area, uint8_t * color_p )
{
  uint32_t w = ( area->x2 - area->x1 + 1 );
  uint32_t h = ( area->y2 - area->y1 + 1 );

  tft.startWrite();
  tft.setAddrWindow( area->x1, area->y1, w, h );
  tft.myPushColors( color_p, w * h * 3, false );
  tft.endWrite();

  lv_disp_flush_ready( disp );
}

static void customTouchpadRead( lv_indev_t * indev_driver, lv_indev_data_t * data )
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

static void tabEventCb( lv_event_t * event ) {
  if( touchEvent ) {  // buzz only on user events (exclude SW events)
    BUZZ_Add( 80 );
    touchEvent = false;
  }

  OTA_LogWrite( "TAB_EVENT\n" );
}

static void touchEventCb( lv_event_t * event ) {
  touchEvent = true;
  lv_timer_reset( timer_setDefaultTab );
  OTA_LogWrite( "TOUCH_EVENT\n" );
}

static void timeEventCb( lv_event_t * event ) {
  if( touchEvent ) {  // buzz only on user events (exclude SW triggered events)
    BUZZ_Add( 80 );
    touchEvent = false;
  }

  OTA_LogWrite( "TIME_EVENT\n" );
  rollerCreate( ROLLER_TIME );
}

static void tempEventCb( lv_event_t * event ) {
  if( touchEvent ) {  // buzz only on user events (exclude SW triggered events)
    BUZZ_Add( 80 );
    touchEvent = false;
  }

  OTA_LogWrite( "TEMP_EVENT\n" );
  rollerCreate( ROLLER_TEMP );
}

static void btnOkEventCb( lv_event_t * event ) {
  if( touchEvent ) {  // buzz only on user events (exclude SW triggered events)
    BUZZ_Add( 80 );
    touchEvent = false;
  }

  roller_t * rType = (roller_t *)lv_event_get_user_data( event );

  OTA_LogWrite( "OK_EVENT\n" );

  if( ROLLER_TEMP == *rType ) {
    uint32_t t1 = lv_roller_get_selected( roller1 );
    uint32_t t2 = lv_roller_get_selected( roller2 );
    uint32_t t3 = lv_roller_get_selected( roller3 );

    rollerTemp = (uint16_t)(t1 * 100 + t2 * 10 + t3);

    if( NULL != tempChangedCB ) {
      tempChangedCB( rollerTemp );
    }
  }
  else if( ROLLER_TIME == *rType ) {
    uint32_t h1 = lv_roller_get_selected( roller1 );
    uint32_t h2 = lv_roller_get_selected( roller2 );
    uint32_t m1 = lv_roller_get_selected( roller3 );
    uint32_t m2 = lv_roller_get_selected( roller4 );

    rollerTime = HOUR_TO_MILLIS(h1 * 10 + h2) + MINUTE_TO_MILLIS(m1 * 10 + m2);

    if( NULL != timeChangedCB ) {
      timeChangedCB( rollerTime );
    }
  }

  lv_obj_del( containerRoller );
}

static void btnCancelEventCb( lv_event_t * event ) {
  if( touchEvent ) {  // buzz only on user events (exclude SW triggered events)
    BUZZ_Add( 80 );
    touchEvent = false;
  }

  OTA_LogWrite( "CANCEL_EVENT\n" );
  lv_obj_del( containerRoller );
}

static void btnStartEventCb( lv_event_t * event ) {
  if( touchEvent ) {  // buzz only on user events (exclude SW triggered events)
    BUZZ_Add( 80 );
    touchEvent = false;
  }

  OTA_LogWrite( "START_EVENT\n" );

  if( NULL != heatingStartCB ) {
    heatingStartCB();
  }
}

static void btnStopEventCb( lv_event_t * event ) {
  if( touchEvent ) {  // buzz only on user events (exclude SW triggered events)
    BUZZ_Add( 80 );
    touchEvent = false;
  }

  OTA_LogWrite( "STOP_EVENT\n" );

  if( NULL != heatingStopCB ) {
    heatingStopCB();
  }
}

static void btnPauseEventCb( lv_event_t * event ) {
  if( touchEvent ) {  // buzz only on user events (exclude SW triggered events)
    BUZZ_Add( 80 );
    touchEvent = false;
  }

  OTA_LogWrite( "PAUSE_EVENT\n" );

  if( NULL != heatingPauseCB ) {
    heatingPauseCB();
  }
}

static void btnBakeSelectEventCb( lv_event_t * event ) {
  if( touchEvent ) {  // buzz only on user events (exclude SW triggered events)
    BUZZ_Add( 80 );
    touchEvent = false;
  }

  OTA_LogWrite( "BAKE_PICKUP_EVENT\n" );

  if( NULL != bakePickupCB ) {
    bakePickupCB( (int32_t)lv_event_get_user_data( event ) );
  }
}

static void rollerCreate( roller_t rType ) {
  #define ROLLER_WIDTH      60
  #define ROLLER_ROW_COUNT  4

  const char * opts2 = "0\n1\n2";
  const char * opts5 = "0\n1\n2\n3\n4\n5";
  const char * opts9 = "0\n1\n2\n3\n4\n5\n6\n7\n8\n9";
  static lv_style_t style_sel;
  static roller_t rollerType;

  ( void )opts2;  // suppress CppCheck warning (c-cpp-flylint)(variableScope)
  rollerType = rType;

  containerRoller = lv_obj_create( tabHome );
  lv_obj_set_style_border_width( containerRoller, 4, 0 );
  lv_obj_set_style_width( containerRoller, lv_obj_get_style_width( tabHome, LV_PART_MAIN ), 0 );
  lv_obj_set_style_height( containerRoller, lv_obj_get_style_height( tabHome, LV_PART_MAIN ), 0 );
  lv_obj_remove_flag( containerRoller, LV_OBJ_FLAG_SCROLLABLE );

  lv_style_init( &style_sel );
  lv_style_set_text_font( &style_sel, &lv_font_montserrat_32 );
  lv_style_set_bg_color( &style_sel, lv_color_hex3(0xf88) );
  lv_style_set_border_width( &style_sel, 2 );
  lv_style_set_border_color( &style_sel, lv_color_hex3(0xf00) );

  roller1 = lv_roller_create( containerRoller );
  lv_roller_set_visible_row_count( roller1, ROLLER_ROW_COUNT );
  lv_obj_set_style_text_font( roller1, &lv_font_ubuntu_regular_24, LV_PART_MAIN );
  lv_obj_set_width( roller1, ROLLER_WIDTH );
  lv_obj_add_style( roller1, &style_sel, LV_PART_SELECTED );

  roller2 = lv_roller_create( containerRoller );
  lv_roller_set_options( roller2, opts9, LV_ROLLER_MODE_NORMAL );
  lv_roller_set_visible_row_count( roller2, ROLLER_ROW_COUNT );
  lv_obj_set_style_text_font( roller2, &lv_font_ubuntu_regular_24, LV_PART_MAIN );
  lv_obj_set_width( roller2, ROLLER_WIDTH );
  lv_obj_add_style( roller2, &style_sel, LV_PART_SELECTED );

  roller3 = lv_roller_create( containerRoller );
  lv_roller_set_visible_row_count( roller3, ROLLER_ROW_COUNT );
  lv_obj_set_style_text_font( roller3, &lv_font_ubuntu_regular_24, LV_PART_MAIN );
  lv_obj_set_width( roller3, ROLLER_WIDTH );
  lv_obj_add_style( roller3, &style_sel, LV_PART_SELECTED );

  if( ROLLER_TEMP == rollerType ) {
    lv_roller_set_options( roller1, opts2, LV_ROLLER_MODE_NORMAL );
    lv_roller_set_options( roller3, opts9, LV_ROLLER_MODE_NORMAL );
    lv_obj_align( roller1, LV_ALIGN_LEFT_MID, 50, -35 );
    lv_obj_align( roller2, LV_ALIGN_LEFT_MID, 135, -35 );
    lv_obj_align( roller3, LV_ALIGN_LEFT_MID, 220, -35 );

    uint32_t rTemp = rollerTemp;
    uint8_t t1 = (uint16_t)(rTemp / 100);
    rTemp -= ( t1 * 100 );
    uint8_t t2 = (uint16_t)(rTemp / 10);
    rTemp -= ( t2 * 10 );
    uint8_t t3 = rTemp;

    lv_roller_set_selected( roller1, (uint32_t)t1, LV_ANIM_OFF );
    lv_roller_set_selected( roller2, (uint32_t)t2, LV_ANIM_OFF );
    lv_roller_set_selected( roller3, (uint32_t)t3, LV_ANIM_OFF );
  }
  else if( ROLLER_TIME == rollerType ) {  // ROLLER_TIME
    lv_roller_set_options( roller1, opts9, LV_ROLLER_MODE_NORMAL );
    lv_roller_set_options( roller3, opts5, LV_ROLLER_MODE_NORMAL );
    lv_obj_align( roller1, LV_ALIGN_LEFT_MID, 10, -35 );
    lv_obj_align( roller2, LV_ALIGN_LEFT_MID, 90, -35 );
    lv_obj_align( roller3, LV_ALIGN_LEFT_MID, 180, -35 );

    roller4 = lv_roller_create( containerRoller );
    lv_roller_set_options( roller4, opts9, LV_ROLLER_MODE_NORMAL );
    lv_roller_set_visible_row_count( roller4, ROLLER_ROW_COUNT );
    lv_obj_set_style_text_font( roller4, &lv_font_ubuntu_regular_24, LV_PART_MAIN );
    lv_obj_set_width( roller4, ROLLER_WIDTH );
    lv_obj_add_style( roller4, &style_sel, LV_PART_SELECTED );
    lv_obj_align( roller4, LV_ALIGN_LEFT_MID, 260, -35 );

    uint32_t rTime = rollerTime;
    uint32_t h1 = (uint32_t)(rTime / HOUR_TO_MILLIS(10));
    rTime -= ( h1 * HOUR_TO_MILLIS(10) );
    uint32_t h2 = (uint32_t)(rTime / HOUR_TO_MILLIS(1));
    rTime -= ( h2 * HOUR_TO_MILLIS(1) );
    uint32_t m1 = (uint32_t)(rTime / MINUTE_TO_MILLIS(10));
    rTime -= ( m1 * MINUTE_TO_MILLIS(10) );
    uint32_t m2 = (uint32_t)(rTime / MINUTE_TO_MILLIS(1));

    lv_roller_set_selected( roller1, (uint32_t)h1, LV_ANIM_OFF );
    lv_roller_set_selected( roller2, (uint32_t)h2, LV_ANIM_OFF );
    lv_roller_set_selected( roller3, (uint32_t)m1, LV_ANIM_OFF );
    lv_roller_set_selected( roller4, (uint32_t)m2, LV_ANIM_OFF );
  }

  // buttons: OK & Cancel
  lv_obj_t * btnOk = lv_button_create( containerRoller );
  lv_obj_t * btnCancel = lv_button_create( containerRoller );
  static lv_style_t styleBtn;

  lv_style_init( &styleBtn );
  lv_style_set_width( &styleBtn, 150 );
  lv_style_set_height( &styleBtn, 50 );
  lv_obj_add_style( btnOk, &styleBtn, 0 );
  lv_obj_add_style( btnCancel, &styleBtn, 0 );
  lv_obj_set_style_bg_color( btnOk, LV_COLOR_MAKE(0x50, 0xAF, 0x4C), 0 );
  lv_obj_set_style_bg_color( btnCancel, LV_COLOR_MAKE(0x36, 0x43, 0xF4), 0 );
  lv_obj_align( btnOk, LV_ALIGN_TOP_LEFT, 5, 205 );
  lv_obj_align( btnCancel, LV_ALIGN_TOP_LEFT, 175, 205 );
  lv_obj_remove_flag( btnOk, LV_OBJ_FLAG_PRESS_LOCK );
  lv_obj_remove_flag( btnCancel, LV_OBJ_FLAG_PRESS_LOCK );
  lv_obj_add_event_cb( btnOk, btnOkEventCb, LV_EVENT_CLICKED, &rollerType );
  lv_obj_add_event_cb( btnCancel, btnCancelEventCb, LV_EVENT_CLICKED, NULL );

  lv_obj_t * labelBtn = lv_label_create( btnOk );
  lv_label_set_text( labelBtn, "OK" );
  lv_obj_center( labelBtn );
  labelBtn = lv_label_create( btnCancel );
  lv_label_set_text( labelBtn, "CANCEL" );
  lv_obj_center( labelBtn );
}

static void createOperatingButtons() {
  if( NULL != containerButtons ) {
    lv_obj_del( containerButtons );
  }
  containerButtons = lv_obj_create( tabHome );
  lv_obj_remove_style_all( containerButtons );
  lv_obj_set_style_width( containerButtons, lv_obj_get_style_width( tabHome, LV_PART_MAIN ), 0 );
  lv_obj_set_style_height( containerButtons, 100, 0 );
  lv_obj_set_style_border_width( containerButtons, 1, 0 );
  lv_obj_set_style_border_color( containerButtons, {0, 0, 255}, 0 );
  lv_obj_set_style_pad_all( containerButtons, 2, LV_PART_MAIN );
  lv_obj_set_style_bg_opa( containerButtons, LV_OPA_0, 0 );
  lv_obj_remove_flag( containerButtons, LV_OBJ_FLAG_SCROLLABLE );
  lv_obj_align( containerButtons, LV_ALIGN_OUT_LEFT_TOP, 0, 200 );

  if( BUTTONS_START == buttonsGroup ) {
    lv_obj_t * btnStart = lv_button_create( containerButtons );
    lv_obj_set_style_width( btnStart, lv_obj_get_style_width( containerButtons, LV_PART_MAIN ), 0 );
    lv_obj_set_style_height( btnStart, lv_obj_get_style_height( containerButtons, LV_PART_MAIN ) - 6, 0 );
    lv_obj_set_style_bg_color( btnStart, LV_COLOR_MAKE(0x50, 0xAF, 0x4C), 0 );
    lv_obj_remove_flag( btnStart, LV_OBJ_FLAG_PRESS_LOCK );
    lv_obj_add_event_cb( btnStart, btnStartEventCb, LV_EVENT_CLICKED, NULL );

    lv_obj_t * labelBtnStart = lv_label_create( btnStart );
    lv_label_set_text( labelBtnStart, "START" );
    lv_obj_center( labelBtnStart );
  }
  else if( ( BUTTONS_PAUSE_STOP == buttonsGroup ) || ( BUTTONS_CONTINUE_STOP == buttonsGroup ) ) {
    lv_obj_t * btnPause = lv_button_create( containerButtons );
    lv_obj_t * btnStop = lv_button_create( containerButtons );
    lv_obj_set_style_width( btnPause, 180, 0 );
    lv_obj_set_style_height( btnPause, lv_obj_get_style_height( containerButtons, LV_PART_MAIN ) - 6, 0 );
    lv_obj_set_style_width( btnStop, 180, 0 );
    lv_obj_set_style_height( btnStop, lv_obj_get_style_height( containerButtons, LV_PART_MAIN ) - 6, 0 );
    lv_obj_set_style_bg_color( btnPause, LV_COLOR_MAKE(0x50, 0xAF, 0x4C), 0 );
    lv_obj_set_style_bg_color( btnStop, {0xF0, 0x20, 0x20}, 0 );
    lv_obj_remove_flag( btnPause, LV_OBJ_FLAG_PRESS_LOCK );
    lv_obj_remove_flag( btnStop, LV_OBJ_FLAG_PRESS_LOCK );
    lv_obj_align( btnPause, LV_ALIGN_TOP_LEFT, 0, 0 );
    lv_obj_align( btnStop, LV_ALIGN_TOP_LEFT, 200, 0 );

    lv_obj_t * labelBtnPause = lv_label_create( btnPause );
    lv_obj_t * labelBtnStop = lv_label_create( btnStop );

    if( BUTTONS_CONTINUE_STOP == buttonsGroup ) {
      lv_label_set_text( labelBtnPause, "CONTINUE" );
    }
    else {
      lv_label_set_text( labelBtnPause, "PAUSE" );
    }

    lv_label_set_text( labelBtnStop, "STOP" );
    lv_obj_center( labelBtnPause );
    lv_obj_center( labelBtnStop );
    lv_obj_add_event_cb( btnPause, btnPauseEventCb, LV_EVENT_CLICKED, NULL );
    lv_obj_add_event_cb( btnStop, btnStopEventCb, LV_EVENT_CLICKED, NULL );
  }
}

static void setContentHome() {
  static lv_style_t styleTabHome;
  static lv_style_t styleTime, styleBtn;

  lv_obj_set_style_bg_color( tabHome, {0xff, 0x00, 0x00}, LV_PART_MAIN ); // red
  lv_obj_set_style_bg_opa( tabHome, LV_OPA_COVER, LV_PART_MAIN );
  lv_obj_set_style_pad_all( tabHome, 5, LV_PART_MAIN );

  /*Add content to the tabs*/
  lv_obj_t * labelTime = lv_label_create( tabHome );
  lv_obj_t * labelTemp = lv_label_create( tabHome );
  btnTime = lv_button_create( tabHome );
  btnTemp = lv_button_create( tabHome );

  // lv_label_set_text( labelTime, "[00:00]\nTime[h:m]\n00:00" ); // used for adjusting label position
  // lv_label_set_text( labelTemp, "[---]\nTemp[°C]\n123" );      // used for adjusting label position
  lv_label_set_text( labelTime, "\nTime[h:m]\n" );
  lv_label_set_text( labelTemp, "\nTemp[°C]\n" );

  lv_obj_align( labelTime, LV_ALIGN_TOP_LEFT, 5, 5 );
  lv_obj_align( labelTemp, LV_ALIGN_TOP_LEFT, 200, 5 );

  labelTargetTimeVal = lv_label_create( tabHome );
  labelCurrentTimeVal = lv_label_create( tabHome );
  labelTargetTempVal = lv_label_create( tabHome );
  labelCurrentTempVal = lv_label_create( tabHome );

  lv_label_set_text( labelTargetTimeVal, "[00:00]" );
  lv_label_set_text( labelCurrentTimeVal, "00:00" );
  lv_label_set_text( labelTargetTempVal, "[---]" );
  lv_label_set_text( labelCurrentTempVal, "123" );

  lv_obj_align( labelTargetTimeVal, LV_ALIGN_TOP_MID, -101, 19 );
  lv_obj_align( labelCurrentTimeVal, LV_ALIGN_TOP_MID, -101, 89 );
  lv_obj_align( labelTargetTempVal, LV_ALIGN_TOP_MID, 94, 19 );
  lv_obj_align( labelCurrentTempVal, LV_ALIGN_TOP_MID, 95, 89 );
  
  lv_style_init( &styleTime );
  lv_style_set_bg_opa( &styleTime, LV_OPA_COVER );
  lv_style_set_radius( &styleTime, 10 );
  lv_style_set_width( &styleTime, 180 );
  lv_style_set_height( &styleTime, 130 );
  lv_style_set_bg_color( &styleTime, lv_palette_lighten(LV_PALETTE_BROWN, 2) );
  lv_style_set_border_width( &styleTime, 4 );
  lv_style_set_border_color( &styleTime, lv_palette_main(LV_PALETTE_BLUE) );
  lv_style_set_pad_ver( &styleTime, 10 );
  lv_style_set_pad_hor( &styleTime, 0 );
  lv_style_set_text_color( &styleTime, lv_palette_main(LV_PALETTE_GREEN) );
  lv_style_set_text_align( &styleTime, LV_TEXT_ALIGN_CENTER );
  lv_obj_add_style( labelTime, &styleTime, 0 );
  lv_obj_add_style( labelTemp, &styleTime, 0 );

  // font size & etc
  lv_style_init( &styleTabHome );
  lv_style_set_text_decor( &styleTabHome, LV_TEXT_DECOR_NONE );
  lv_style_set_text_font( &styleTabHome, &lv_font_montserrat_32 );
  lv_style_set_pad_all( &styleTabHome, 15 );
  lv_obj_add_style( tabHome, &styleTabHome, 0 );

  // trick: invisible button overlapping label (just to have clickable label)
  lv_style_init( &styleBtn );
  lv_style_set_bg_opa( &styleBtn, LV_OPA_0 );
  lv_style_set_radius( &styleBtn, 10 );
  lv_style_set_width( &styleBtn, 180 );
  lv_style_set_height( &styleBtn, 130 );
  lv_style_set_border_width( &styleBtn, 4 );
  lv_style_set_border_color( &styleBtn, LV_COLOR_MAKE(0xF3, 0x96, 0x21) );// lv_palette_main(LV_PALETTE_BLUE) >> 2196F3
  lv_style_set_border_opa( &styleBtn, LV_OPA_COVER );
  lv_obj_remove_style_all( btnTime );
  lv_obj_remove_style_all( btnTemp );
  lv_obj_add_style( btnTime, &styleBtn, 0 );
  lv_obj_add_style( btnTemp, &styleBtn, 0 );
  lv_obj_align( btnTime, LV_ALIGN_TOP_LEFT, 5, 5 );
  lv_obj_align( btnTemp, LV_ALIGN_TOP_LEFT, 200, 5 );
  lv_obj_remove_flag( btnTime, LV_OBJ_FLAG_PRESS_LOCK );
  lv_obj_remove_flag( btnTemp, LV_OBJ_FLAG_PRESS_LOCK );
  GUI_setTimeTempChangeAllowed( true );

  buttonsGroup = BUTTONS_START; // show Start button by default
  createOperatingButtons();
}

static void setContentList( char *nameList, uint32_t nameLength, uint32_t nameCount ) {
  static lv_style_t styleTabList;

  lv_obj_set_style_bg_color( tabList, {0x00, 0x00, 0x00}, 0 );
  lv_obj_set_style_bg_opa( tabList, LV_OPA_COVER, 0 );
  lv_obj_set_style_pad_all( tabList, 0, LV_PART_MAIN );

  // font size & etc
  lv_style_init( &styleTabList );
  lv_style_set_text_decor( &styleTabList, LV_TEXT_DECOR_NONE );
  lv_style_set_text_font( &styleTabList, &lv_font_ubuntu_regular_24 );
  lv_obj_add_style( tabList, &styleTabList, 0 );

  /*Create a list*/
  bakeList = lv_list_create( tabList );
  lv_obj_set_size( bakeList, lv_obj_get_style_width( tabList, LV_PART_MAIN ), lv_obj_get_style_height( tabList, LV_PART_MAIN ) );
  lv_obj_center( bakeList );
  lv_obj_set_style_radius( bakeList, 0, LV_PART_MAIN );
  lv_obj_remove_flag( bakeList, LV_OBJ_FLAG_SCROLL_ELASTIC );
  lv_obj_remove_flag( bakeList, LV_OBJ_FLAG_SCROLL_MOMENTUM );

  if( NULL != nameList && 1000 > nameCount ) {  // max 999 positions on the list allowed
    for( int x=0; x<nameCount; x++ ) {
      lv_obj_t * btn;
      char buffer[ nameLength+5 ];  // additional 5 bytes for 3 digits number and 2 static chars ": "

      /*Add buttons to the list*/
      snprintf( buffer, nameLength, "%d: %s", (x+1), (nameList + nameLength * x) );
      btn = lv_list_add_button( bakeList, LV_SYMBOL_RIGHT_ARROW, buffer );  // special character available on lv_font_ubuntu_regular_24 only
      lv_obj_add_event_cb( btn, btnBakeSelectEventCb, LV_EVENT_CLICKED, (void *)x );  // use pointer as ordinary value
    }
  }
}

static void setContentOptions() {
  static lv_style_t styleTabOptions;

  lv_obj_set_style_bg_color( tabOptions, {0x00, 0x00, 0xff}, 0 ); // blue
  lv_obj_set_style_bg_opa( tabOptions, LV_OPA_COVER, 0 );

  /*Add content to the tabs*/
  lv_obj_t * label = lv_label_create( tabOptions );
  lv_label_set_text( label, "Third tab (00F blue)" );

  // font size & etc
  lv_style_init( &styleTabOptions );
  lv_style_set_text_decor( &styleTabOptions, LV_TEXT_DECOR_NONE );
  lv_style_set_text_font( &styleTabOptions, &lv_font_ubuntu_regular_24 );
  lv_obj_add_style( tabOptions, &styleTabOptions, 0 );
}

static void setScreenMain() {
  /*Create a Tab view object*/
  tabView = lv_tabview_create( lv_scr_act() );
  lv_tabview_set_tab_bar_position( tabView, LV_DIR_RIGHT );
  lv_tabview_set_tab_bar_size( tabView, 80 );

  lv_obj_t * tab_buttons = lv_tabview_get_tab_bar( tabView );
  lv_obj_set_style_bg_color( tab_buttons, lv_palette_darken(LV_PALETTE_GREY, 3), 0 );
  lv_obj_set_style_text_color( tab_buttons, lv_palette_lighten(LV_PALETTE_GREY, 3), 0 );
  lv_obj_set_style_border_side( tab_buttons, LV_BORDER_SIDE_RIGHT, LV_PART_MAIN );

  lv_style_init( &styleTabs );
  lv_style_set_text_decor( &styleTabs, LV_TEXT_DECOR_NONE );
  lv_style_set_text_font( &styleTabs, &lv_font_montserrat_32 );
  lv_obj_add_style( tabView, &styleTabs, 0 );

  // Add 3 tabs (returned tabs are page (widget of lv_page) and can be scrolled
  tabHome = lv_tabview_add_tab( tabView, LV_SYMBOL_HOME );
  tabList = lv_tabview_add_tab( tabView, LV_SYMBOL_LIST );
  tabOptions = lv_tabview_add_tab( tabView, LV_SYMBOL_SETTINGS );

  lv_obj_remove_flag( lv_tabview_get_content(tabView), LV_OBJ_FLAG_SCROLLABLE );
  // set callback for clicking on TABs
  lv_obj_add_event_cb( tabView, tabEventCb, LV_EVENT_VALUE_CHANGED, NULL );

  setContentHome();
  setContentList( NULL, 0, 0 );
  setContentOptions();

  // create frame around the whole screen
  // top bar
  heatingIndicator1 = lv_button_create( lv_scr_act() );
  lv_obj_set_size( heatingIndicator1, LV_HOR_RES, 15 );
  lv_obj_align( heatingIndicator1, LV_ALIGN_TOP_MID, 0, 0 );
  lv_obj_set_style_radius( heatingIndicator1, 0, LV_PART_MAIN );
  lv_obj_set_style_border_width( heatingIndicator1, 0, LV_PART_MAIN );
  lv_obj_set_style_bg_opa( heatingIndicator1, LV_OPA_TRANSP, LV_PART_MAIN );
  lv_obj_set_style_bg_color( heatingIndicator1, {0x00, 0xff, 0x00}, LV_PART_MAIN );  //green
  lv_obj_set_style_shadow_width( heatingIndicator1, 0, LV_PART_MAIN );
  lv_obj_clear_flag( heatingIndicator1, LV_OBJ_FLAG_CLICKABLE );
  // right bar
  heatingIndicator2 = lv_button_create( lv_scr_act() );
  lv_obj_set_size( heatingIndicator2, 15, LV_VER_RES );
  lv_obj_align( heatingIndicator2, LV_ALIGN_RIGHT_MID, 0, 0 );
  lv_obj_set_style_radius( heatingIndicator2, 0, LV_PART_MAIN );
  lv_obj_set_style_border_width( heatingIndicator2, 0, LV_PART_MAIN );
  lv_obj_set_style_bg_opa( heatingIndicator2, LV_OPA_TRANSP, LV_PART_MAIN );
  lv_obj_set_style_bg_color( heatingIndicator2, {0x00, 0xff, 0x00}, LV_PART_MAIN );  //green
  lv_obj_set_style_shadow_width( heatingIndicator2, 0, LV_PART_MAIN );
  lv_obj_clear_flag( heatingIndicator2, LV_OBJ_FLAG_CLICKABLE );
  // bottom bar
  heatingIndicator3 = lv_button_create( lv_scr_act() );
  lv_obj_set_size( heatingIndicator3, LV_HOR_RES, 15 );
  lv_obj_align( heatingIndicator3, LV_ALIGN_BOTTOM_MID, 0, 0 );
  lv_obj_set_style_radius( heatingIndicator3, 0, LV_PART_MAIN );
  lv_obj_set_style_border_width( heatingIndicator3, 0, LV_PART_MAIN );
  lv_obj_set_style_bg_opa( heatingIndicator3, LV_OPA_TRANSP, LV_PART_MAIN );
  lv_obj_set_style_bg_color( heatingIndicator3, {0x00, 0xff, 0x00}, LV_PART_MAIN );  //green
  lv_obj_set_style_shadow_width( heatingIndicator3, 0, LV_PART_MAIN );
  lv_obj_clear_flag( heatingIndicator3, LV_OBJ_FLAG_CLICKABLE );
  // left bar
  heatingIndicator4 = lv_button_create( lv_scr_act() );
  lv_obj_set_size( heatingIndicator4, 15, LV_VER_RES );
  lv_obj_align( heatingIndicator4, LV_ALIGN_LEFT_MID, 0, 0 );
  lv_obj_set_style_radius( heatingIndicator4, 0, LV_PART_MAIN );
  lv_obj_set_style_border_width( heatingIndicator4, 0, LV_PART_MAIN );
  lv_obj_set_style_bg_opa( heatingIndicator4, LV_OPA_TRANSP, LV_PART_MAIN );
  lv_obj_set_style_bg_color( heatingIndicator4, {0x00, 0xff, 0x00}, LV_PART_MAIN );  //green
  lv_obj_set_style_shadow_width( heatingIndicator4, 0, LV_PART_MAIN );
  lv_obj_clear_flag( heatingIndicator4, LV_OBJ_FLAG_CLICKABLE );
}

static void blinkTimeCurrent( lv_timer_t * timer ) {//( TimerHandle_t timer ) {
  static bool isVisible = true;

  ( void )isVisible;  // suppress CppCheck warning (variableScope)

  if( NULL != labelCurrentTimeVal ) {
    Serial.println("BLINK_TIME_TOGGLE");
    if( isVisible ){
      lv_obj_set_style_text_color( labelCurrentTimeVal, {0xD3,0xD3,0xD3}, 0 );
      isVisible = false;
    }
    else {
      lv_obj_set_style_text_color( labelCurrentTimeVal, {0x0,0x0,0x0}, 0 );
      isVisible = true;
    }
  }
}

static void blinkScreenFrame( lv_timer_t * timer ) {
  static bool isVisible = true;

  ( void )isVisible;  // suppress CppCheck warning (variableScope)

  if( NULL != heatingIndicator1
  &&  NULL != heatingIndicator2
  &&  NULL != heatingIndicator3
  &&  NULL != heatingIndicator4 ) {
    Serial.println("BLINK_FRAME_TOGGLE");
    if( isVisible ){
      lv_obj_set_style_bg_opa( heatingIndicator1, LV_OPA_COVER, LV_PART_MAIN );
      lv_obj_set_style_bg_opa( heatingIndicator2, LV_OPA_COVER, LV_PART_MAIN );
      lv_obj_set_style_bg_opa( heatingIndicator3, LV_OPA_COVER, LV_PART_MAIN );
      lv_obj_set_style_bg_opa( heatingIndicator4, LV_OPA_COVER, LV_PART_MAIN );
      isVisible = false;
    }
    else {
      lv_obj_set_style_bg_opa( heatingIndicator1, LV_OPA_TRANSP, LV_PART_MAIN );
      lv_obj_set_style_bg_opa( heatingIndicator2, LV_OPA_TRANSP, LV_PART_MAIN );
      lv_obj_set_style_bg_opa( heatingIndicator3, LV_OPA_TRANSP, LV_PART_MAIN );
      lv_obj_set_style_bg_opa( heatingIndicator4, LV_OPA_TRANSP, LV_PART_MAIN );
      isVisible = true;
    }
  }
}

static void setDefaultTab( lv_timer_t * timer ) {
  GUI_SetTabActive( 0 );
}

void GUI_Init() {
    
  uint16_t calData[5] = { 265, 3677, 261, 3552, 1 };  // check branch TouchscreenCalibration for those values
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

  lv_indev_add_event_cb( indev, touchEventCb, LV_EVENT_CLICKED, NULL );

  setScreenMain();

  timer_blinkTimeCurrent = lv_timer_create( blinkTimeCurrent, 250,  NULL );
  lv_timer_pause( timer_blinkTimeCurrent );
  timer_blinkScreenFrame = lv_timer_create( blinkScreenFrame, 500,  NULL );
  lv_timer_pause( timer_blinkScreenFrame );
  timer_setDefaultTab = lv_timer_create( setDefaultTab, 10000,  NULL );
  lv_timer_enable( timer_setDefaultTab );
}

void GUI_Handle( uint32_t tick_period ) {
  lv_timer_handler();
  lv_tick_inc( 10 );
}

void GUI_SetTabActive( uint32_t tabNr )
{
  if( (0 > tabNr) || (3 <= tabNr) ) {
    return;
  }

  lv_tabview_set_active( tabView, tabNr, LV_ANIM_OFF );
  Serial.println( "Timer callback function executed" );
}

void GUI_SetTargetTemp( uint16_t temp ) {
  char buff[6];
  uint16_t t = temp;
  uint16_t t1, t2, t3;

  rollerTemp = t;

  t1 = (uint16_t)(t / 100);
  t -= ( t1 * 100 );
  t2 = (uint16_t)(t / 10);
  t -= ( t2 * 10 );
  t3 = t;

  buff[0] = '[';
  buff[1] = ( 0 < t1 ? '0' + t1 : ' ' );
  buff[2] = (( 0 < t2 ) || ( buff[0] != ' ' )) ? '0' + t2 : ' ';  //'0' + t2;
  buff[3] = '0' + t3;
  buff[4] = ']';
  buff[5] = '\0';

  lv_label_set_text( labelTargetTempVal, buff );
  // lv_label_set_text( labelTargetTempVal, "[---]" );  // used for adjusting label position
}

void GUI_SetCurrentTemp( uint16_t temp ) {
  char buff[4];
  ///TODO: double code!, duplication in GUI_SetTargetTemp(), can be reduced to one function call. The same regarding time
  uint16_t t = temp;
  uint16_t t1, t2, t3;

  if( 999 < t ) { // no more as 3 digits
    t = 999;
  }

  t1 = (uint16_t)(t / 100);
  t -= ( t1 * 100 );
  t2 = (uint16_t)(t / 10);
  t -= ( t2 * 10 );
  t3 = t;

  buff[0] = ( 0 < t1 ? '0' + t1 : ' ' );
  buff[1] = '0' + t2;
  buff[2] = '0' + t3;
  buff[3] = '\0';

  lv_label_set_text( labelCurrentTempVal, buff );
  // lv_label_set_text( labelCurrentTempVal, "123" );  // used for adjusting label position
}

void GUI_SetTargetTime( uint32_t time ) {
  char buff[8];
  uint32_t t = time;
  uint32_t h1, h2, m1, m2;

  if( MAX_ALLOWED_TIME < t ) {
    t = MAX_ALLOWED_TIME;
  }

  rollerTime = t;

  h1 = (uint32_t)(t / HOUR_TO_MILLIS(10));
  t -= ( h1 * HOUR_TO_MILLIS(10) );
  h2 = (uint32_t)(t / HOUR_TO_MILLIS(1));
  t -= ( h2 * HOUR_TO_MILLIS(1) );
  m1 = (uint32_t)(t / MINUTE_TO_MILLIS(10));
  t -= ( m1 * MINUTE_TO_MILLIS(10) );
  m2 = (uint32_t)(t / MINUTE_TO_MILLIS(1));

  buff[0] = '[';
  buff[1] = ( 0 < h1 ? '0' + h1 : ' ' );
  buff[2] = '0' + h2;
  buff[3] = ':';
  buff[4] = '0' + m1;
  buff[5] = '0' + m2;
  buff[6] = ']';
  buff[7] = '\0';

  lv_label_set_text( labelTargetTimeVal, buff );
  // lv_label_set_text( labelTargetTimeVal, "[00:00]" );  // used for adjusting label position
}

void GUI_SetCurrentTime( uint32_t time ) {
  char buff[6];
  uint32_t t = time;
  uint32_t h1, h2, m1, m2;

  if( MAX_ALLOWED_TIME < t ) {
    t = MAX_ALLOWED_TIME;
  }

  h1 = (uint32_t)(t / HOUR_TO_MILLIS(10));
  t -= ( h1 * HOUR_TO_MILLIS(10) );
  h2 = (uint32_t)(t / HOUR_TO_MILLIS(1));
  t -= ( h2 * HOUR_TO_MILLIS(1) );
  m1 = (uint32_t)(t / MINUTE_TO_MILLIS(10));
  t -= ( m1 * MINUTE_TO_MILLIS(10) );
  m2 = (uint32_t)(t / MINUTE_TO_MILLIS(1));

  buff[0] = ( 0 < h1 ? '0' + h1 : ' ' );
  buff[1] = '0' + h2;
  buff[2] = ':';
  buff[3] = '0' + m1;
  buff[4] = '0' + m2;
  buff[5] = '\0';

  if( NULL != labelCurrentTimeVal ) {
    lv_label_set_text( labelCurrentTimeVal, buff );
    // lv_label_set_text( labelCurrentTimeVal, "00:00" );  // used for adjusting label position
  }
}

void GUI_setTimeCallback( updateTimeCb func ) {
  if( NULL != func ) {
    timeChangedCB = func;
  }
}

void GUI_setTempCallback( updateTempCb func ) {
  if( NULL != func ) {
    tempChangedCB = func;
  }
}

void GUI_setStartCallback( operationCb func ) {
  if( NULL != func ) {
    heatingStartCB = func;
  }
}

void GUI_setStopCallback( operationCb func ) {
  if( NULL != func ) {
    heatingStopCB = func;
  }
}

void GUI_setPauseCallback( operationCb func ) {
  if( NULL != func ) {
    heatingPauseCB = func;
  }
}

void GUI_setBakePickupCallback( bakePickupCb func ) {
  if( NULL != func ) {
    bakePickupCB = func;
  }
}

void GUI_setOperationButtons( enum operationButton btnGroup ) {
  if( BUTTONS_MAX_COUNT > btnGroup ) {
    buttonsGroup = btnGroup;
    createOperatingButtons();
  }
  else {
    buttonsGroup = (buttonsGroup_t)0; // wrong enum received
  }
}

void GUI_setTimeTempChangeAllowed( bool active ) {
  if( active ) {
    lv_obj_add_event_cb( btnTime, timeEventCb, LV_EVENT_CLICKED, NULL );
    lv_obj_add_event_cb( btnTemp, tempEventCb, LV_EVENT_CLICKED, NULL );
  }
  else {
    lv_obj_remove_event_cb( btnTime, timeEventCb );
    lv_obj_remove_event_cb( btnTemp, tempEventCb );
  }
}

void GUI_setBlinkTimeCurrent( bool active ) {
  if( NULL == timer_blinkTimeCurrent ) {
    return;
  }

  if( NULL != labelCurrentTimeVal ) {
    if( active ) {
      lv_timer_resume( timer_blinkTimeCurrent );
      Serial.println("BLINK_TIME_START");
    }
    else {
      lv_timer_pause( timer_blinkTimeCurrent );
      // set proper label color here, just in case it's changed when stopping blinking
      lv_obj_set_style_text_color( labelCurrentTimeVal, {0x0,0x0,0x0}, 0 );
      Serial.println("BLINK_TIME_STOP");
    }
  }
}

void GUI_setBlinkScreenFrame( bool active ) {
  if( NULL == timer_blinkScreenFrame ) {
    return;
  }

  if( NULL != heatingIndicator1
  &&  NULL != heatingIndicator2
  &&  NULL != heatingIndicator3
  &&  NULL != heatingIndicator4 ) {

    if( active ) {
      lv_timer_resume( timer_blinkScreenFrame );
      Serial.println("BLINK_FRAME_START");
    }
    else {
      lv_timer_pause( timer_blinkScreenFrame );
      lv_obj_set_style_bg_opa( heatingIndicator1, LV_OPA_TRANSP, LV_PART_MAIN );
      lv_obj_set_style_bg_opa( heatingIndicator2, LV_OPA_TRANSP, LV_PART_MAIN );
      lv_obj_set_style_bg_opa( heatingIndicator3, LV_OPA_TRANSP, LV_PART_MAIN );
      lv_obj_set_style_bg_opa( heatingIndicator4, LV_OPA_TRANSP, LV_PART_MAIN );
      Serial.println("BLINK_FRAME_STOP");
    }
  }
}

SPIClass * GUI_getSPIinstance() {
  return &(tft.getSPIinstance());
}

void GUI_populateBakeListNames( char *nameList, uint32_t nameLength, uint32_t nameCount ) {
  setContentList( nameList, nameLength, nameCount );
}
