#include "gui.h"
#include "TFT_eSPI.h"
#include "lvgl.h"
#include "buzzer.h"
#include "myOTA.h"

#define TERMOMETER_BAR_MIN    -30
#define TERMOMETER_BAR_MAX    115

typedef enum rollerType { ROLLER_TIME = 1, ROLLER_TEMP } roller_t;
typedef enum bakeOperationType { BAKE_NONE = 0, BAKE_REMOVE, BAKE_SWAP } bakeOperation_t;

static lv_obj_t * tabView;    // main container for 3 tabs
static lv_style_t styleTabs;  // has impact on tabs icons size
static lv_obj_t * tabHome;    // the widget where the content of the tab HOME can be created
static lv_obj_t * tabList;    // the widget where the content of the tab LIST can be created
static lv_obj_t * tabOptions; // the widget where the content of the tab OPTIONS can be created
static lv_obj_t * widgetTime;
static lv_obj_t * widgetTemp;
static lv_obj_t * progressCircle;
static lv_obj_t * tempBar;
static lv_obj_t * labelBakeName;
static lv_obj_t * labelSoundIcon;
static lv_obj_t * labelWiFiIcon;
static lv_obj_t * powerBar;
static lv_obj_t * labelPowerBar;
static lv_style_t styleScreenFrame;
static bool       touchEvent = false;
static lv_obj_t * labelTargetTempVal;
static lv_obj_t * labelTargetTimeVal;
static lv_obj_t * labelCurrentTempVal;
static lv_obj_t * labelCurrentTimeVal;
static lv_obj_t * containerRoller;
static lv_obj_t * containerButtons;
static lv_obj_t * containerBakesRemoval;
static lv_obj_t * roller1;
static lv_obj_t * roller2;
static lv_obj_t * roller3;
static lv_obj_t * roller4;
static lv_obj_t * bakeList;
static lv_obj_t * optionList;
static lv_obj_t * msgBox;
static updateTimeCb timeChangedCB = NULL;
static updateTempCb tempChangedCB = NULL;
static operationCb heatingStartCB = NULL;
static operationCb heatingStopCB = NULL;
static operationCb heatingPauseCB = NULL;
static bakePickupCb bakePickupCB = NULL;
static adjustTimeCb adjustTimeCB = NULL;
static removeBakesCb removeBakesCB = NULL;
static swapBakesCb swapBakesCB = NULL;
static buttonsGroup_t buttonsGroup;
static uint16_t rollerTemp;
static uint32_t rollerTime;
static lv_timer_t * timer_blinkTimeCurrent;
static lv_timer_t * timer_blinkScreenFrame;
static lv_timer_t * timer_setDefaultTab;
uint8_t bakesToRemoveList[ BAKES_TO_REMOVE_MAX ];   // used for swaping two bakes also
const char defaultBakeName[] = "Manual operation";
static bakeOperationType bakeOperation;

static SemaphoreHandle_t  xSemaphore = NULL;
static StaticSemaphore_t  xMutexBuffer;
static uint32_t inEventHandling = 0;

TFT_eSPI tft = TFT_eSPI();
static lv_color_t buf[LV_HOR_RES_MAX * LV_VER_RES_MAX / 10]; // Declare a buffer for 1/10 screen size

static void customDisplayFlush( lv_display_t * disp, const lv_area_t * area, uint8_t * color_p );
static void customTouchpadRead( lv_indev_t * indev_driver, lv_indev_data_t * data );
static void tabEventCb( lv_event_t * event );
static void touchEventCb( lv_event_t * event );
static void pressingEventCb( lv_event_t * event );
static void timeEventCb( lv_event_t * event );
static void tempEventCb( lv_event_t * event );
static void btnOkEventCb( lv_event_t * event );
static void btnCancelEventCb( lv_event_t * event );
static void btnStartEventCb( lv_event_t * event );
static void btnStopEventCb( lv_event_t * event );
static void btnPauseEventCb( lv_event_t * event );
static void btnBakeSelectEventCb( lv_event_t * event );
static void btnOptionEventCb( lv_event_t * event );
static void btnOptionAdjustTimeEventCb( lv_event_t * event );
static void btnOptionRemoveBakesEventCb( lv_event_t * event );
static void btnBakesRemovalCancelEventCb( lv_event_t * event );
static void btnBakesRemovalDeleteEventCb( lv_event_t * event );
static void btnBakesSwapEventCb( lv_event_t * event );
static void checkboxChangedEventCb( lv_event_t * event );
static void msgBoxOkEventCb( lv_event_t * event );
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

static void pressingEventCb( lv_event_t * event ) {
  lv_timer_reset( timer_setDefaultTab );  // prevent default tab activation when scrolling/pressing bakeList
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
      inEventHandling++;
      tempChangedCB( rollerTemp );
      inEventHandling--;
    }
  }
  else if( ROLLER_TIME == *rType ) {
    uint32_t h1 = lv_roller_get_selected( roller1 );
    uint32_t h2 = lv_roller_get_selected( roller2 );
    uint32_t m1 = lv_roller_get_selected( roller3 );
    uint32_t m2 = lv_roller_get_selected( roller4 );

    rollerTime = HOUR_TO_MILLIS(h1 * 10 + h2) + MINUTE_TO_MILLIS(m1 * 10 + m2);

    if( NULL != timeChangedCB ) {
      inEventHandling++;
      timeChangedCB( rollerTime );
      inEventHandling--;
    }
  }

  // manual settings remove previously selected bake name
  lv_label_set_text( labelBakeName, defaultBakeName );

  lv_obj_delete( containerRoller );
  containerRoller = NULL;  // LVGL bug? pointer is not NULL here
}

static void btnCancelEventCb( lv_event_t * event ) {
  if( touchEvent ) {  // buzz only on user events (exclude SW triggered events)
    BUZZ_Add( 80 );
    touchEvent = false;
  }

  OTA_LogWrite( "CANCEL_EVENT\n" );
  lv_obj_delete( containerRoller );
  containerRoller = NULL;  // LVGL bug? pointer is not NULL here
}

static void btnStartEventCb( lv_event_t * event ) {
  if( touchEvent ) {  // buzz only on user events (exclude SW triggered events)
    BUZZ_Add( 80 );
    touchEvent = false;
  }

  OTA_LogWrite( "START_EVENT\n" );

  if( NULL != heatingStartCB ) {
    inEventHandling++;
    heatingStartCB();
    inEventHandling--;
  }
}

static void btnStopEventCb( lv_event_t * event ) {
  if( touchEvent ) {  // buzz only on user events (exclude SW triggered events)
    BUZZ_Add( 80 );
    touchEvent = false;
  }

  OTA_LogWrite( "STOP_EVENT\n" );

  if( NULL != heatingStopCB ) {
    inEventHandling++;
    heatingStopCB();
    inEventHandling--;
  }
}

static void btnPauseEventCb( lv_event_t * event ) {
  if( touchEvent ) {  // buzz only on user events (exclude SW triggered events)
    BUZZ_Add( 80 );
    touchEvent = false;
  }

  OTA_LogWrite( "PAUSE_EVENT\n" );

  if( NULL != heatingPauseCB ) {
    inEventHandling++;
    heatingPauseCB();
    inEventHandling--;
  }
}

static void btnBakeSelectEventCb( lv_event_t * event ) {
  lv_event_code_t code = lv_event_get_code( event );

  if( touchEvent ) {  // buzz only on user events (exclude SW triggered events)
    BUZZ_Add( 80 );
    touchEvent = false;
  }

  OTA_LogWrite( "BAKE_PICKUP_EVENT\n" );

  if( NULL != bakePickupCB ) {
    inEventHandling++;
    bakePickupCB( (int32_t)lv_event_get_user_data( event ), ( LV_EVENT_LONG_PRESSED == code ) );
    inEventHandling--;
  }
}

static void btnOptionEventCb( lv_event_t * event ) {
  if( touchEvent ) {  // buzz only on user events (exclude SW triggered events)
    BUZZ_Add( 80 );
    touchEvent = false;
  }
  
  setting_t *data = (setting_t *)lv_event_get_user_data( event );
  (void)data;
  
  if( NULL != data ) {
    if( data->optionCallback ) {
      inEventHandling++;
      data->optionCallback();
      inEventHandling--;
    }
  }
}

static void btnOptionAdjustTimeEventCb( lv_event_t * event ) {
  if( touchEvent ) {  // buzz only on user events (exclude SW triggered events)
    BUZZ_Add( 80 );
    touchEvent = false;
  }
  
  if( NULL != adjustTimeCB ) {
    inEventHandling++;
    adjustTimeCB( (int32_t)lv_event_get_user_data( event ) );
    inEventHandling--;
  }
}

static void btnOptionRemoveBakesEventCb( lv_event_t * event ) {
  if( containerBakesRemoval ) {
    lv_event_stop_processing( event );
    return;
  }

  if( touchEvent ) {  // buzz only on user events (exclude SW triggered events)
    BUZZ_Add( 80 );
    touchEvent = false;
  }

  // clear list with bakes indexes
  for( int x=0; x<BAKES_TO_REMOVE_MAX; x++ ) {
    bakesToRemoveList[ x ] = 0;
  }

  // create container for bake list
  containerBakesRemoval = lv_obj_create( tabOptions );
  lv_obj_set_style_border_width( containerBakesRemoval, 4, 0 );
  lv_obj_set_style_width( containerBakesRemoval, lv_obj_get_style_width( tabOptions, LV_PART_MAIN )-3, LV_PART_MAIN );
  lv_obj_set_style_height( containerBakesRemoval, lv_obj_get_style_height( tabOptions, LV_PART_MAIN )-3, LV_PART_MAIN );
  lv_obj_set_style_radius( containerBakesRemoval, 10, LV_PART_MAIN );
  lv_obj_set_style_bg_color( containerBakesRemoval, lv_palette_darken(LV_PALETTE_GREY, 3), LV_PART_MAIN );
  lv_obj_set_style_bg_opa( containerBakesRemoval, LV_OPA_80, LV_PART_MAIN );
  lv_obj_set_style_border_width( containerBakesRemoval, 2, LV_PART_MAIN );
  lv_obj_set_style_border_color( containerBakesRemoval, lv_palette_darken(LV_PALETTE_GREY, 3), LV_PART_MAIN );
  lv_obj_set_style_border_opa( containerBakesRemoval, LV_OPA_90, LV_PART_MAIN );
  lv_obj_align( containerBakesRemoval, LV_ALIGN_CENTER, 0, lv_obj_get_scroll_y( tabOptions ) );
  lv_obj_remove_flag( containerBakesRemoval, LV_OBJ_FLAG_SCROLLABLE );
  lv_obj_remove_flag( containerBakesRemoval, LV_OBJ_FLAG_SCROLL_CHAIN );

  lv_obj_t * containerBakesList = lv_obj_create( containerBakesRemoval );
  lv_obj_set_style_border_width( containerBakesList, 0, LV_PART_MAIN );
  lv_obj_set_style_bg_opa( containerBakesList, LV_OPA_TRANSP, LV_PART_MAIN );
  lv_obj_set_style_pad_all( containerBakesList, 0, LV_PART_MAIN );
  lv_obj_set_style_width( containerBakesList, 0, LV_PART_SCROLLBAR );
  lv_obj_set_style_width( containerBakesList, lv_obj_get_style_width( containerBakesRemoval, LV_PART_MAIN )+2, LV_PART_MAIN );
  lv_obj_set_style_height( containerBakesList, lv_obj_get_style_height( containerBakesRemoval, LV_PART_MAIN )-7, LV_PART_MAIN );
  lv_obj_align( containerBakesList, LV_ALIGN_TOP_MID, 0, -15 );
  lv_obj_remove_flag( containerBakesList, LV_OBJ_FLAG_SCROLL_ELASTIC );
  lv_obj_remove_flag( containerBakesList, LV_OBJ_FLAG_SCROLL_MOMENTUM );
  lv_obj_set_scroll_dir( containerBakesList, LV_DIR_VER );

  // populate container with bake names
  lv_obj_t * btn;
  uint32_t idx = 0;
  while( btn = lv_obj_get_child( bakeList, idx ) ) {
    lv_obj_t * cb = lv_checkbox_create( containerBakesList );
    lv_obj_t * label = lv_obj_get_child( btn, 1 );  // index 0: ICON, index 1: LABEL of the button

    if( label ) {
      lv_checkbox_set_text( cb, lv_label_get_text( label ) );
    } else {
      lv_checkbox_set_text( cb, "..." );
    }
    lv_obj_set_style_text_color( cb, {0xE0, 0xE0, 0xE0}, LV_PART_MAIN );
    lv_obj_add_event_cb( cb, checkboxChangedEventCb, LV_EVENT_VALUE_CHANGED, (void *)(idx+1) ); // counts elements from 1
    lv_obj_align( cb, LV_ALIGN_TOP_LEFT, 0, idx*35 );

    idx++;
  }

  // QUIRK: treat pointer as value
  bakeOperation_t bo = (bakeOperation_t)(uint32_t)lv_event_get_user_data( event );

  // buttons: DELETE/SWAP & Cancel
  lv_obj_t * btnOk = lv_button_create( containerBakesRemoval );
  lv_obj_t * btnCancel = lv_button_create( containerBakesRemoval );
  static lv_style_t styleBtn;

  lv_style_init( &styleBtn );
  lv_style_set_width( &styleBtn, 170 );
  lv_style_set_height( &styleBtn, 50 );
  lv_style_set_shadow_width( &styleBtn, 0 );
  lv_style_set_border_color( &styleBtn, lv_color_hex3(0x000) );
  lv_style_set_border_opa( &styleBtn, LV_OPA_70 );
  lv_style_set_border_width( &styleBtn, 2 );
  lv_obj_add_style( btnOk, &styleBtn, 0 );
  lv_obj_add_style( btnCancel, &styleBtn, 0 );
  lv_obj_set_style_bg_color( btnOk, LV_COLOR_MAKE(0x50, 0xAF, 0x4C), 0 );
  lv_obj_set_style_bg_color( btnCancel, LV_COLOR_MAKE(0x36, 0x43, 0xF4), 0 );
  lv_obj_align( btnCancel, LV_ALIGN_TOP_LEFT, 0, 234 );
  lv_obj_align( btnOk, LV_ALIGN_TOP_LEFT, 180, 234 );
  lv_obj_remove_flag( btnOk, LV_OBJ_FLAG_PRESS_LOCK );
  lv_obj_remove_flag( btnCancel, LV_OBJ_FLAG_PRESS_LOCK );
  lv_obj_add_event_cb( btnCancel, btnBakesRemovalCancelEventCb, LV_EVENT_CLICKED, NULL );
  if( BAKE_REMOVE == bo ) {
    bakeOperation = BAKE_REMOVE;
    lv_obj_add_event_cb( btnOk, btnBakesRemovalDeleteEventCb, LV_EVENT_CLICKED, NULL );
  } else if( BAKE_SWAP == bo ) {
    bakeOperation = BAKE_SWAP;
    lv_obj_add_event_cb( btnOk, btnBakesSwapEventCb, LV_EVENT_CLICKED, NULL );
  } else {
    bakeOperation = BAKE_NONE;
  }

  lv_obj_t * labelBtn = lv_label_create( btnOk );
  if( BAKE_REMOVE == bo ) {
    lv_label_set_text( labelBtn, "DELETE" );
  } else if( BAKE_SWAP == bo ) {
    lv_label_set_text( labelBtn, "SWAP" );
  }
  lv_obj_center( labelBtn );
  labelBtn = lv_label_create( btnCancel );
  lv_label_set_text( labelBtn, "CANCEL" );
  lv_obj_center( labelBtn );
}

static void btnBakesRemovalCancelEventCb( lv_event_t * event ) {
  if( touchEvent ) {  // buzz only on user events (exclude SW triggered events)
    BUZZ_Add( 80 );
    touchEvent = false;
  }

  OTA_LogWrite( "CANCEL_EVENT\n" );

  lv_event_stop_processing( event );
  lv_obj_delete( containerBakesRemoval );
  containerBakesRemoval = NULL;   // LVGL bug? pointer is not NULL here
  msgBox = NULL;                  // LVGL bug? pointer is not NULL here
}

static void btnBakesRemovalDeleteEventCb( lv_event_t * event ) {
  if( touchEvent ) {  // buzz only on user events (exclude SW triggered events)
    BUZZ_Add( 80 );
    touchEvent = false;
  }
  
  if( containerBakesRemoval ) {
    lv_event_stop_processing( event );
    lv_obj_delete( containerBakesRemoval );
    containerBakesRemoval = NULL;   // LVGL bug? pointer is not NULL here
    msgBox = NULL;                  // LVGL bug? pointer is not NULL here
  }
  
  if( NULL != removeBakesCB ) {
    inEventHandling++;
    removeBakesCB( &bakesToRemoveList[0] );
    inEventHandling--;
  }

  // clear list with bakes indexes
  for( int x=0; x<BAKES_TO_REMOVE_MAX; x++ ) {
    bakesToRemoveList[ x ] = 0;
  }
}

static void btnBakesSwapEventCb( lv_event_t * event ) {
  if( touchEvent ) {  // buzz only on user events (exclude SW triggered events)
    BUZZ_Add( 80 );
    touchEvent = false;
  }
  
  if( containerBakesRemoval ) {
    lv_event_stop_processing( event );
    lv_obj_delete( containerBakesRemoval );
    containerBakesRemoval = NULL;   // LVGL bug? pointer is not NULL here
    msgBox = NULL;                  // LVGL bug? pointer is not NULL here
  }
  
  if( NULL != swapBakesCB ) {
    inEventHandling++;
    swapBakesCB( &bakesToRemoveList[0] );
    inEventHandling--;
  }

  // clear list with bakes indexes
  for( int x=0; x<BAKES_TO_REMOVE_MAX; x++ ) {
    bakesToRemoveList[ x ] = 0;
  }
}

/**
 * one function for handling removal and swaping
 */
static void checkboxChangedEventCb( lv_event_t * event ) {
  if( touchEvent ) {  // buzz only on user events (exclude SW triggered events)
    BUZZ_Add( 80 );
    touchEvent = false;
  }
  
  lv_obj_t * obj = lv_event_get_target_obj( event );
  // QUIRK: treat pointer as value
  uint8_t newIdx = (uint8_t)(uint32_t)lv_event_get_user_data( event );
  uint8_t maxCheckedBakes = BAKES_TO_REMOVE_MAX;
  
  if( BAKE_SWAP == bakeOperation ) {
    maxCheckedBakes = 2;
  }
  
  if( LV_STATE_CHECKED & lv_obj_get_state(obj) ) {
    // checkbox is checked, add idx to list
    bool found = false;
    Serial.printf( "Element to be added to list:%d\n", newIdx );
    
    for( int x=0; x<maxCheckedBakes; x++ ) {
      if( 0 == bakesToRemoveList[x] ) {
        bakesToRemoveList[x] = newIdx;
        found = true;
        break;
      }
    }
    if( false == found ) {  // full list, can't add more
      Serial.println( "Too much elements checked." );
      lv_obj_remove_state( obj, LV_STATE_CHECKED );
      // display GUI messageBox about max checked positions
      if( NULL == msgBox ) {
        char msg[50];
        sprintf( msg, " You can check\n %d positions max.", maxCheckedBakes );

        msgBox = lv_msgbox_create( containerBakesRemoval );
        lv_msgbox_add_title( msgBox, "Information:" );
        lv_msgbox_add_text( msgBox, msg );
        lv_obj_t * btn = lv_msgbox_add_footer_button( msgBox, "Ok" );
        lv_obj_add_event_cb( btn, msgBoxOkEventCb, LV_EVENT_CLICKED, NULL );
      }
    }
  } else {
    // checkbox is unchecked, remove idx from list
    Serial.printf( "Element to be removed from list:%d\n", newIdx );
    
    for( int x=0; x<maxCheckedBakes; x++ ) {
      if( newIdx == bakesToRemoveList[x] ) {
        bakesToRemoveList[x] = 0;
        break;
      }
    }
  }
}

static void msgBoxOkEventCb( lv_event_t * event ) {
  lv_obj_t * btn = lv_event_get_target_obj( event );
  lv_obj_t * mbox = lv_obj_get_parent( lv_obj_get_parent( btn ) );
  lv_msgbox_close( mbox );
  msgBox = NULL;    // LVGL bug? pointer is not NULL here
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
  lv_obj_set_style_width( containerRoller, lv_obj_get_style_width( tabHome, LV_PART_MAIN )-3, 0 );
  lv_obj_set_style_height( containerRoller, lv_obj_get_style_height( tabHome, LV_PART_MAIN )-3, 0 );
  lv_obj_set_style_radius( containerRoller, 10, LV_PART_MAIN );
  lv_obj_set_style_bg_color( containerRoller, lv_palette_darken(LV_PALETTE_GREY, 3), LV_PART_MAIN );
  lv_obj_set_style_bg_opa( containerRoller, LV_OPA_80, LV_PART_MAIN );
  lv_obj_set_style_border_width( containerRoller, 2, LV_PART_MAIN );
  lv_obj_set_style_border_color( containerRoller, lv_palette_darken(LV_PALETTE_GREY, 3), LV_PART_MAIN );
  lv_obj_set_style_border_opa( containerRoller, LV_OPA_90, LV_PART_MAIN );
  lv_obj_align( containerRoller, LV_ALIGN_CENTER, 0, 1 );
  lv_obj_remove_flag( containerRoller, LV_OBJ_FLAG_SCROLLABLE );

  lv_style_init( &style_sel );
  lv_style_set_height( &style_sel, lv_obj_get_style_height( containerRoller, LV_PART_MAIN )-70 );
  lv_style_set_text_font( &style_sel, &lv_font_montserrat_custom_32 );
  lv_style_set_bg_color( &style_sel, lv_color_hex3(0xf88) );
  lv_style_set_border_width( &style_sel, 0 );

  roller1 = lv_roller_create( containerRoller );
  lv_roller_set_visible_row_count( roller1, ROLLER_ROW_COUNT );
  lv_obj_set_style_text_font( roller1, &lv_font_montserrat_custom_24, LV_PART_MAIN );
  lv_obj_set_style_bg_opa( roller1, LV_OPA_60, LV_PART_MAIN );
  lv_obj_set_style_border_color( roller1, lv_color_hex3(0x000), LV_PART_MAIN );
  lv_obj_set_style_border_opa( roller1, LV_OPA_70, LV_PART_MAIN );
  lv_obj_set_width( roller1, ROLLER_WIDTH );
  lv_obj_add_style( roller1, &style_sel, LV_PART_SELECTED );

  roller2 = lv_roller_create( containerRoller );
  lv_roller_set_options( roller2, opts9, LV_ROLLER_MODE_NORMAL );
  lv_roller_set_visible_row_count( roller2, ROLLER_ROW_COUNT );
  lv_obj_set_style_text_font( roller2, &lv_font_montserrat_custom_24, LV_PART_MAIN );
  lv_obj_set_style_bg_opa( roller2, LV_OPA_60, LV_PART_MAIN );
  lv_obj_set_style_border_color( roller2, lv_color_hex3(0x000), LV_PART_MAIN );
  lv_obj_set_style_border_opa( roller2, LV_OPA_70, LV_PART_MAIN );
  lv_obj_set_width( roller2, ROLLER_WIDTH );
  lv_obj_add_style( roller2, &style_sel, LV_PART_SELECTED );

  roller3 = lv_roller_create( containerRoller );
  lv_roller_set_visible_row_count( roller3, ROLLER_ROW_COUNT );
  lv_obj_set_style_text_font( roller3, &lv_font_montserrat_custom_24, LV_PART_MAIN );
  lv_obj_set_style_bg_opa( roller3, LV_OPA_60, LV_PART_MAIN );
  lv_obj_set_style_border_color( roller3, lv_color_hex3(0x000), LV_PART_MAIN );
  lv_obj_set_style_border_opa( roller3, LV_OPA_70, LV_PART_MAIN );
  lv_obj_set_width( roller3, ROLLER_WIDTH );
  lv_obj_add_style( roller3, &style_sel, LV_PART_SELECTED );

  if( ROLLER_TEMP == rollerType ) {
    lv_roller_set_options( roller1, opts2, LV_ROLLER_MODE_NORMAL );
    lv_roller_set_options( roller3, opts9, LV_ROLLER_MODE_NORMAL );
    lv_obj_align( roller1, LV_ALIGN_LEFT_MID, 60, -31 );
    lv_obj_align( roller2, LV_ALIGN_LEFT_MID, 145, -31 );
    lv_obj_align( roller3, LV_ALIGN_LEFT_MID, 230, -31 );

    lv_obj_t * labelTemp = lv_label_create( containerRoller );
    lv_label_set_text( labelTemp, "°C" );
    lv_obj_set_style_text_color( labelTemp, {0xFF, 0xFF, 0xFF}, LV_PART_MAIN );
    lv_obj_align( labelTemp, LV_ALIGN_CENTER, 145, -29 );

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
    lv_obj_align( roller1, LV_ALIGN_LEFT_MID, 15, -31 );
    lv_obj_align( roller2, LV_ALIGN_LEFT_MID, 90, -31 );
    lv_obj_align( roller3, LV_ALIGN_LEFT_MID, 185, -31 );

    roller4 = lv_roller_create( containerRoller );
    lv_roller_set_options( roller4, opts9, LV_ROLLER_MODE_NORMAL );
    lv_roller_set_visible_row_count( roller4, ROLLER_ROW_COUNT );
    lv_obj_set_style_text_font( roller4, &lv_font_montserrat_custom_24, LV_PART_MAIN );
    lv_obj_set_style_bg_opa( roller4, LV_OPA_60, LV_PART_MAIN );
    lv_obj_set_style_border_color( roller4, lv_color_hex3(0x000), LV_PART_MAIN );
    lv_obj_set_style_border_opa( roller4, LV_OPA_70, LV_PART_MAIN );
    lv_obj_set_width( roller4, ROLLER_WIDTH );
    lv_obj_add_style( roller4, &style_sel, LV_PART_SELECTED );
    lv_obj_align( roller4, LV_ALIGN_LEFT_MID, 260, -31 );

    lv_obj_t * labelHour = lv_label_create( containerRoller );
    lv_obj_t * labelMinute = lv_label_create( containerRoller );
    lv_label_set_text( labelHour, "h" );
    lv_label_set_text( labelMinute, "m" );
    lv_obj_set_style_text_color( labelHour, {0xFF, 0xFF, 0xFF}, LV_PART_MAIN );
    lv_obj_set_style_text_color( labelMinute, {0xFF, 0xFF, 0xFF}, LV_PART_MAIN );
    lv_obj_align( labelHour, LV_ALIGN_CENTER, -10, -30 );
    lv_obj_align( labelMinute, LV_ALIGN_CENTER, 167, -30 );

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
  lv_style_set_width( &styleBtn, 170 );
  lv_style_set_height( &styleBtn, 50 );
  lv_style_set_shadow_width( &styleBtn, 0 );
  lv_style_set_border_color( &styleBtn, lv_color_hex3(0x000) );
  lv_style_set_border_opa( &styleBtn, LV_OPA_70 );
  lv_style_set_border_width( &styleBtn, 2 );
  lv_obj_add_style( btnOk, &styleBtn, 0 );
  lv_obj_add_style( btnCancel, &styleBtn, 0 );
  lv_obj_set_style_bg_color( btnOk, LV_COLOR_MAKE(0x50, 0xAF, 0x4C), 0 );
  lv_obj_set_style_bg_color( btnCancel, LV_COLOR_MAKE(0x36, 0x43, 0xF4), 0 );
  lv_obj_align( btnCancel, LV_ALIGN_TOP_LEFT, 0, 234 );
  lv_obj_align( btnOk, LV_ALIGN_TOP_LEFT, 180, 234 );
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
    lv_obj_delete( containerButtons );
    containerButtons = NULL;  // LVGL bug? pointer is not NULL here
  }
  containerButtons = lv_obj_create( tabHome );
  lv_obj_remove_style_all( containerButtons );
  lv_obj_set_style_width( containerButtons, lv_obj_get_style_width( tabHome, LV_PART_MAIN ), 0 );
  lv_obj_set_style_height( containerButtons, 100, 0 );
  lv_obj_set_style_border_width( containerButtons, 0, 0 );
  lv_obj_set_style_pad_all( containerButtons, 2, LV_PART_MAIN );
  lv_obj_set_style_bg_opa( containerButtons, LV_OPA_0, 0 );
  lv_obj_remove_flag( containerButtons, LV_OBJ_FLAG_SCROLLABLE );
  lv_obj_align( containerButtons, LV_ALIGN_OUT_LEFT_TOP, 0, 220 );

  if( BUTTONS_START == buttonsGroup ) {
    lv_obj_t * btnStart = lv_button_create( containerButtons );
    lv_obj_set_style_width( btnStart, lv_obj_get_style_width( containerButtons, LV_PART_MAIN ) - 3, 0 );
    lv_obj_set_style_height( btnStart, lv_obj_get_style_height( containerButtons, LV_PART_MAIN ) - 10, 0 );
    lv_obj_set_style_bg_color( btnStart, LV_COLOR_MAKE(0x50, 0xAF, 0x4C), 0 );
    lv_obj_set_style_outline_width( btnStart, 2, LV_PART_MAIN );
    lv_obj_set_style_outline_color( btnStart, lv_color_hex3(0x000), LV_PART_MAIN );
    lv_obj_set_style_outline_opa( btnStart, LV_OPA_70, LV_PART_MAIN );
    lv_obj_set_style_shadow_width( btnStart, 0, LV_PART_MAIN );
    lv_obj_align( btnStart, LV_ALIGN_CENTER, 0, 0 );
    lv_obj_remove_flag( btnStart, LV_OBJ_FLAG_PRESS_LOCK );
    lv_obj_add_event_cb( btnStart, btnStartEventCb, LV_EVENT_CLICKED, NULL );

    lv_obj_t * labelBtnStart = lv_label_create( btnStart );
    lv_label_set_text( labelBtnStart, "START" );
    lv_obj_center( labelBtnStart );
  }
  else if( ( BUTTONS_PAUSE_STOP == buttonsGroup ) || ( BUTTONS_CONTINUE_STOP == buttonsGroup ) ) {
    lv_obj_t * btnPause = lv_button_create( containerButtons );
    lv_obj_t * btnStop = lv_button_create( containerButtons );
    lv_obj_set_style_width( btnPause, 186, 0 );
    lv_obj_set_style_height( btnPause, lv_obj_get_style_height( containerButtons, LV_PART_MAIN ) - 10, 0 );
    lv_obj_set_style_width( btnStop, 186, 0 );
    lv_obj_set_style_height( btnStop, lv_obj_get_style_height( containerButtons, LV_PART_MAIN ) - 10, 0 );
    lv_obj_set_style_bg_color( btnPause, LV_COLOR_MAKE(0x50, 0xAF, 0x4C), 0 );
    lv_obj_set_style_bg_color( btnStop, {0xF0, 0x20, 0x20}, 0 );
    lv_obj_set_style_outline_width( btnPause, 2, LV_PART_MAIN );
    lv_obj_set_style_outline_color( btnPause, lv_color_hex3(0x000), LV_PART_MAIN );
    lv_obj_set_style_outline_opa( btnPause, LV_OPA_70, LV_PART_MAIN );
    lv_obj_set_style_outline_width( btnStop, 2, LV_PART_MAIN );
    lv_obj_set_style_outline_color( btnStop, lv_color_hex3(0x000), LV_PART_MAIN );
    lv_obj_set_style_outline_opa( btnStop, LV_OPA_70, LV_PART_MAIN );
    lv_obj_set_style_shadow_width( btnPause, 0, LV_PART_MAIN );
    lv_obj_set_style_shadow_width( btnStop, 0, LV_PART_MAIN );
    lv_obj_remove_flag( btnPause, LV_OBJ_FLAG_PRESS_LOCK );
    lv_obj_remove_flag( btnStop, LV_OBJ_FLAG_PRESS_LOCK );
    lv_obj_align( btnPause, LV_ALIGN_CENTER, -99, 0 );
    lv_obj_align( btnStop, LV_ALIGN_CENTER, 99, 0 );

    lv_obj_t * labelBtnPause = lv_label_create( btnPause );
    lv_obj_t * labelBtnStop = lv_label_create( btnStop );

    if( BUTTONS_CONTINUE_STOP == buttonsGroup ) {
      lv_label_set_text( labelBtnPause, "CONTIN." );
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
  lv_obj_set_style_bg_color( tabHome, lv_palette_darken(LV_PALETTE_GREY, 2), LV_PART_MAIN );
  lv_obj_set_style_bg_opa( tabHome, LV_OPA_COVER, LV_PART_MAIN );
  lv_obj_set_style_pad_all( tabHome, 0, LV_PART_MAIN );
  lv_obj_set_style_border_width( tabHome, 0, 0 );
  lv_obj_set_style_text_decor( tabHome, LV_TEXT_DECOR_NONE, 0 );
  lv_obj_set_style_text_font( tabHome, &lv_font_montserrat_custom_38, 0 );

  // clickable time's widget
  widgetTime = lv_button_create( tabHome );
  lv_obj_set_style_bg_color( widgetTime, lv_palette_darken(LV_PALETTE_GREY, 3), LV_PART_MAIN );
  lv_obj_set_style_bg_opa( widgetTime, LV_OPA_20, LV_PART_MAIN );
  lv_obj_set_style_radius( widgetTime, 20, LV_PART_MAIN );
  lv_obj_set_style_width( widgetTime, 150, LV_PART_MAIN );
  lv_obj_set_style_height( widgetTime, 150, LV_PART_MAIN );
  lv_obj_set_style_border_width( widgetTime, 2, LV_PART_MAIN );
  lv_obj_set_style_border_color( widgetTime, lv_palette_darken(LV_PALETTE_GREY, 3), LV_PART_MAIN );// lv_palette_main(LV_PALETTE_BLUE) >> 2196F3
  lv_obj_set_style_border_opa( widgetTime, LV_OPA_40, LV_PART_MAIN );
  lv_obj_set_style_shadow_width( widgetTime, 0, LV_PART_MAIN );
  lv_obj_align( widgetTime, LV_ALIGN_TOP_LEFT, 15, 15 );
  lv_obj_remove_flag( widgetTime, LV_OBJ_FLAG_PRESS_LOCK );

  progressCircle = lv_arc_create( widgetTime );
  lv_arc_set_rotation( progressCircle, 270 );
  lv_arc_set_bg_angles( progressCircle, 0, 360 );
  lv_arc_set_range( progressCircle, 0, 1000 );
  lv_obj_set_style_width( progressCircle, 140, LV_PART_MAIN );
  lv_obj_set_style_height( progressCircle, 140, LV_PART_MAIN );
  lv_obj_set_style_arc_color( progressCircle, lv_palette_darken(LV_PALETTE_GREY, 3), LV_PART_MAIN );
  lv_obj_set_style_arc_color( progressCircle, {0xff, 0xff, 0xff}, LV_PART_INDICATOR );
  lv_obj_set_style_arc_opa( progressCircle, LV_OPA_70, LV_PART_INDICATOR );
  lv_obj_align( progressCircle, LV_ALIGN_CENTER, 0, 0 );
  lv_obj_remove_style( progressCircle, NULL, LV_PART_KNOB );
  lv_obj_remove_flag( progressCircle, LV_OBJ_FLAG_CLICKABLE );
  lv_arc_set_value( progressCircle, 1000 );

  // current time
  labelCurrentTimeVal = lv_label_create( widgetTime );
  lv_label_set_text( labelCurrentTimeVal, "00:00" );
  lv_obj_set_style_text_color( labelCurrentTimeVal, {0x00, 0x00, 0x00}, LV_PART_MAIN );
  lv_obj_align( labelCurrentTimeVal, LV_ALIGN_CENTER, 0, -10 );
  // target time
  labelTargetTimeVal = lv_label_create( widgetTime );
  lv_label_set_text( labelTargetTimeVal, "[00:00]" );
  lv_obj_set_style_text_color( labelTargetTimeVal, {0x00, 0x00, 0x00}, LV_PART_MAIN );
  lv_obj_set_style_text_font( labelTargetTimeVal, &lv_font_montserrat_custom_24, LV_PART_MAIN );
  lv_obj_align( labelTargetTimeVal, LV_ALIGN_CENTER, 0, 20 );

  // clickable temp's widget
  widgetTemp = lv_button_create( tabHome );
  lv_obj_set_style_bg_color( widgetTemp, lv_palette_darken(LV_PALETTE_GREY, 3), LV_PART_MAIN );
  lv_obj_set_style_bg_opa( widgetTemp, LV_OPA_20, LV_PART_MAIN );
  lv_obj_set_style_radius( widgetTemp, 20, LV_PART_MAIN );
  lv_obj_set_style_width( widgetTemp, 150, LV_PART_MAIN );
  lv_obj_set_style_height( widgetTemp, 150, LV_PART_MAIN );
  lv_obj_set_style_border_width( widgetTemp, 2, LV_PART_MAIN );
  lv_obj_set_style_border_color( widgetTemp, lv_palette_darken(LV_PALETTE_GREY, 3), LV_PART_MAIN );// lv_palette_main(LV_PALETTE_BLUE) >> 2196F3
  lv_obj_set_style_border_opa( widgetTemp, LV_OPA_40, LV_PART_MAIN );
  lv_obj_set_style_shadow_width( widgetTemp, 0, LV_PART_MAIN );
  lv_obj_align( widgetTemp, LV_ALIGN_TOP_LEFT, 180, 15 );
  lv_obj_remove_flag( widgetTemp, LV_OBJ_FLAG_PRESS_LOCK );

  // main part of thermometer (vertical bar)
  tempBar = lv_bar_create( widgetTemp );
  lv_obj_set_size( tempBar, 20, 120 );
  lv_obj_align( tempBar, LV_ALIGN_CENTER, -50, 0 );
  lv_bar_set_range( tempBar, TERMOMETER_BAR_MIN, TERMOMETER_BAR_MAX );
  lv_obj_set_style_bg_opa( tempBar, LV_OPA_COVER, LV_PART_INDICATOR );
  lv_obj_set_style_bg_grad_color( tempBar, {0x00, 0x00, 0xff}, LV_PART_INDICATOR );   // gradient from blue
  lv_obj_set_style_bg_color( tempBar, {0xff, 0x00, 0x00}, LV_PART_INDICATOR );        // to red
  lv_obj_set_style_bg_grad_dir( tempBar, LV_GRAD_DIR_VER, LV_PART_INDICATOR );
  lv_obj_remove_flag( tempBar, LV_OBJ_FLAG_CLICKABLE );
  lv_bar_set_value( tempBar, 25, LV_ANIM_OFF );
  // bottom part of thermometer (the bulb)
  lv_obj_t * bulb  = lv_led_create( widgetTemp );
  lv_obj_align( bulb, LV_ALIGN_CENTER, -50, 55 );
  lv_led_set_color( bulb, {0x00, 0x00, 0xff} );
  lv_led_set_brightness( bulb, 220 );
  lv_obj_set_style_shadow_width( bulb, 0 ,0 );
  lv_obj_remove_flag( bulb, LV_OBJ_FLAG_CLICKABLE );

  // target temp
  labelTargetTempVal = lv_label_create( widgetTemp );
  lv_label_set_text( labelTargetTempVal, "220" );
  lv_obj_set_style_text_color( labelTargetTempVal, {0x00, 0x00, 0x00}, LV_PART_MAIN );
  lv_obj_set_style_text_font( labelTargetTempVal, &lv_font_montserrat_custom_16, LV_PART_MAIN );
  lv_obj_align( labelTargetTempVal, LV_ALIGN_TOP_LEFT, 21, 10 );
  // current temp
  labelCurrentTempVal = lv_label_create( widgetTemp );
  lv_label_set_text( labelCurrentTempVal, "123" );
  lv_obj_set_style_text_color( labelCurrentTempVal, {0x00, 0x00, 0x00}, LV_PART_MAIN );
  lv_obj_align( labelCurrentTempVal, LV_ALIGN_CENTER, 0, 0 );
  // min/max indicators
  lv_obj_t * zeroIndic = lv_label_create( widgetTemp );
  lv_label_set_text( zeroIndic, "--- 0°C" );
  lv_obj_set_style_text_color( zeroIndic, {0x00, 0x00, 0x00}, LV_PART_MAIN );
  lv_obj_set_style_text_font( zeroIndic, &lv_font_montserrat_custom_16, LV_PART_MAIN );
  lv_obj_align( zeroIndic, LV_ALIGN_CENTER, -33, 40 );
  lv_obj_t * targetIndic = lv_label_create( widgetTemp );
  lv_label_set_text( targetIndic, "---" );
  lv_obj_set_style_text_color( targetIndic, {0x00, 0x00, 0x00}, LV_PART_MAIN );
  lv_obj_set_style_text_font( targetIndic, &lv_font_montserrat_custom_16, LV_PART_MAIN );
  lv_obj_align( targetIndic, LV_ALIGN_TOP_LEFT, -2, 10 );

  // power bar
  powerBar = lv_bar_create( tabHome );
  lv_obj_set_size( powerBar, 30, 107 );
  lv_obj_align( powerBar, LV_ALIGN_CENTER, 165, -65 );
  lv_bar_set_range( powerBar, -1, 100 );
  lv_obj_set_style_pad_all( powerBar, 3, LV_PART_MAIN );
  lv_obj_set_style_radius( powerBar, 0, LV_PART_MAIN );
  lv_obj_set_style_radius( powerBar, 0, LV_PART_INDICATOR );
  lv_obj_set_style_bg_opa( powerBar, LV_OPA_40, LV_PART_INDICATOR );
  lv_obj_set_style_bg_color( powerBar, {0xff, 0x00, 0x00}, LV_PART_INDICATOR );   // red
  lv_obj_set_style_border_width( powerBar, 2, LV_PART_MAIN );
  lv_obj_set_style_border_color( powerBar, lv_palette_darken(LV_PALETTE_GREY, 3), LV_PART_MAIN );
  lv_obj_set_style_border_opa( powerBar, LV_OPA_40, LV_PART_MAIN );
  lv_obj_set_style_shadow_width( powerBar, 0, LV_PART_MAIN );
  lv_obj_remove_flag( powerBar, LV_OBJ_FLAG_CLICKABLE );
  lv_bar_set_value( powerBar, 0, LV_ANIM_OFF );

  // label for power bar
  labelPowerBar = lv_label_create( tabHome );
  lv_label_set_text( labelPowerBar, "0%" );
  lv_obj_set_style_text_color( labelPowerBar, {0x00, 0x00, 0x00}, LV_PART_MAIN );
  lv_obj_set_style_text_font( labelPowerBar, &lv_font_montserrat_custom_16, LV_PART_MAIN );
  lv_obj_align( labelPowerBar, LV_ALIGN_CENTER, 167, -1 );

  // label for bake name
  labelBakeName = lv_label_create( tabHome );
  lv_label_set_text( labelBakeName, defaultBakeName );
  lv_label_set_long_mode( labelBakeName, LV_LABEL_LONG_SCROLL_CIRCULAR );
  lv_obj_set_style_text_align( labelBakeName, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN );
  lv_obj_set_style_pad_top( labelBakeName, 3, LV_PART_MAIN );
  lv_obj_set_style_pad_hor( labelBakeName, 1, LV_PART_MAIN );
  lv_obj_set_style_width( labelBakeName, 354, LV_PART_MAIN );
  lv_obj_set_style_height( labelBakeName, 38, LV_PART_MAIN );
  lv_obj_set_style_margin_all( labelBakeName, 0, LV_PART_MAIN );
  lv_obj_set_style_border_width( labelBakeName, 1, LV_PART_MAIN );
  lv_obj_set_style_border_color( labelBakeName, lv_palette_darken(LV_PALETTE_GREY, 3), LV_PART_MAIN );
  lv_obj_set_style_outline_width( labelBakeName, 0, LV_PART_MAIN );
  lv_obj_set_style_outline_pad( labelBakeName, 0, LV_PART_MAIN );
  lv_obj_set_style_shadow_width( labelBakeName, 0, LV_PART_MAIN );
  lv_obj_set_style_bg_color( labelBakeName, lv_palette_darken(LV_PALETTE_GREY, 3), LV_PART_MAIN );
  lv_obj_set_style_bg_opa( labelBakeName, LV_OPA_20, LV_PART_MAIN );
  lv_obj_set_style_radius( labelBakeName, 10, LV_PART_MAIN );
  lv_obj_set_style_text_color( labelBakeName, {0x00, 0x00, 0x00}, LV_PART_MAIN );
  lv_obj_set_style_text_font( labelBakeName, &lv_font_montserrat_custom_24, LV_PART_MAIN );
  lv_obj_align( labelBakeName, LV_ALIGN_CENTER, 0, 33 );

  // sound icon
  labelSoundIcon = lv_label_create( tabHome );
  lv_label_set_text( labelSoundIcon, LV_SYMBOL_VOLUME_MAX );
  lv_obj_set_style_text_color( labelSoundIcon, {0x00, 0x00, 0x00}, LV_PART_MAIN );
  lv_obj_set_style_text_font( labelSoundIcon, &lv_font_montserrat_custom_16, LV_PART_MAIN );
  lv_obj_align( labelSoundIcon, LV_ALIGN_CENTER, 150, -134 );

  // WiFi icon
  labelWiFiIcon = lv_label_create( tabHome );
  lv_label_set_text( labelWiFiIcon, LV_SYMBOL_WIFI );
  lv_obj_set_style_text_color( labelWiFiIcon, {0x00, 0x00, 0x00}, LV_PART_MAIN );
  lv_obj_set_style_text_font( labelWiFiIcon, &lv_font_montserrat_custom_16, LV_PART_MAIN );
  lv_obj_align( labelWiFiIcon, LV_ALIGN_CENTER, 180, -134 );

  buttonsGroup = BUTTONS_START; // show Start button by default
  createOperatingButtons();
  GUI_setTimeTempChangeAllowed( true );
}

static void setContentList( char *nameList, uint32_t nameLength, uint32_t nameCount ) {
  static lv_style_t styleTabList;

  lv_obj_set_style_bg_color( tabList, {0x00, 0x00, 0x00}, 0 );
  lv_obj_set_style_bg_opa( tabList, LV_OPA_COVER, 0 );
  lv_obj_set_style_pad_all( tabList, 0, LV_PART_MAIN );

  // font size & etc
  lv_style_init( &styleTabList );
  lv_style_set_text_decor( &styleTabList, LV_TEXT_DECOR_NONE );
  lv_style_set_text_font( &styleTabList, &lv_font_montserrat_custom_24 );
  lv_obj_add_style( tabList, &styleTabList, 0 );

  /*Create a list*/
  bakeList = lv_list_create( tabList );
  lv_obj_set_size( bakeList, lv_obj_get_style_width( tabList, LV_PART_MAIN ), lv_obj_get_style_height( tabList, LV_PART_MAIN ) );
  lv_obj_center( bakeList );
  lv_obj_set_style_radius( bakeList, 0, LV_PART_MAIN );
  lv_obj_set_style_bg_color( bakeList, lv_palette_darken(LV_PALETTE_GREY, 3), LV_PART_SCROLLBAR );
  lv_obj_set_style_width( bakeList, 15, LV_PART_SCROLLBAR );
  lv_obj_remove_flag( bakeList, LV_OBJ_FLAG_SCROLL_ELASTIC );
  lv_obj_remove_flag( bakeList, LV_OBJ_FLAG_SCROLL_MOMENTUM );
  lv_obj_add_event_cb( bakeList, pressingEventCb, LV_EVENT_PRESSING, NULL );

  if( NULL != nameList && 1000 > nameCount ) {  // max 999 positions on the list allowed
    for( int x=0; x<nameCount; x++ ) {
      lv_obj_t * btn;
      char buffer[ nameLength+5 ];  // additional 5 bytes for 3 digits number and 2 static chars ": "

      /*Add buttons to the list*/
      snprintf( buffer, nameLength, "%d: %s", (x+1), (nameList + nameLength * x) );
      btn = lv_list_add_button( bakeList, LV_SYMBOL_RIGHT, buffer );
      lv_obj_remove_flag( btn, LV_OBJ_FLAG_PRESS_LOCK );
      lv_obj_add_event_cb( btn, btnBakeSelectEventCb, LV_EVENT_SHORT_CLICKED, (void *)x );  // use pointer as ordinary value
      lv_obj_add_event_cb( btn, btnBakeSelectEventCb, LV_EVENT_LONG_PRESSED, (void *)x );   // use pointer as ordinary value
    }
  }
}

static void setContentOptions() {
  static lv_style_t styleTabOptions;

  lv_obj_set_style_bg_color( tabOptions, lv_palette_lighten(LV_PALETTE_GREY, 1), LV_PART_MAIN );
  lv_obj_set_style_bg_opa( tabOptions, LV_OPA_COVER, 0 );
  lv_obj_set_style_pad_all( tabOptions, 0, LV_PART_MAIN );
  lv_obj_set_style_width( tabOptions, 0, LV_PART_SCROLLBAR );
  lv_obj_remove_flag( tabOptions, LV_OBJ_FLAG_SCROLL_ELASTIC );
  lv_obj_remove_flag( tabOptions, LV_OBJ_FLAG_SCROLL_MOMENTUM );

  // font size & etc
  lv_style_init( &styleTabOptions );
  lv_style_set_text_decor( &styleTabOptions, LV_TEXT_DECOR_NONE );
  lv_style_set_text_font( &styleTabOptions, &lv_font_montserrat_custom_24 );
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
  lv_style_set_text_font( &styleTabs, &lv_font_montserrat_custom_32 );
  lv_obj_add_style( tabView, &styleTabs, 0 );

  // Add 3 tabs (returned tabs are page (widget of lv_page) and can be scrolled
  tabHome = lv_tabview_add_tab( tabView, LV_SYMBOL_HOME );
  tabList = lv_tabview_add_tab( tabView, LV_SYMBOL_LIST );
  tabOptions = lv_tabview_add_tab( tabView, LV_SYMBOL_SETTINGS );

  lv_obj_remove_flag( lv_tabview_get_content(tabView), LV_OBJ_FLAG_SCROLLABLE );
  lv_obj_remove_flag( tabHome, LV_OBJ_FLAG_SCROLLABLE );
  // set callback for clicking on TABs
  lv_obj_add_event_cb( tabView, tabEventCb, LV_EVENT_VALUE_CHANGED, NULL );

  setContentHome();
  setContentList( NULL, 0, 0 );
  setContentOptions();

  // create frame around the whole screen
  static lv_obj_t * heatingIndicator1;
  static lv_obj_t * heatingIndicator2;
  static lv_obj_t * heatingIndicator3;
  static lv_obj_t * heatingIndicator4;
  // style properties
  lv_style_init( &styleScreenFrame );
  lv_style_set_radius( &styleScreenFrame, 0 );
  lv_style_set_border_width( &styleScreenFrame, 0 );
  lv_style_set_bg_opa( &styleScreenFrame, LV_OPA_TRANSP );
  lv_style_set_bg_color( &styleScreenFrame, {0x00, 0xff, 0x00} );  //green
  lv_style_set_shadow_width( &styleScreenFrame, 0 );
  // top bar
  heatingIndicator1 = lv_button_create( lv_scr_act() );
  lv_obj_set_size( heatingIndicator1, LV_HOR_RES, 16 );
  lv_obj_align( heatingIndicator1, LV_ALIGN_TOP_MID, 0, 0 );
  lv_obj_add_style( heatingIndicator1, &styleScreenFrame, LV_PART_MAIN );
  lv_obj_clear_flag( heatingIndicator1, LV_OBJ_FLAG_CLICKABLE );
  // right bar
  heatingIndicator2 = lv_button_create( lv_scr_act() );
  lv_obj_set_size( heatingIndicator2, 16, LV_VER_RES-32 );
  lv_obj_align( heatingIndicator2, LV_ALIGN_RIGHT_MID, 0, 0 );
  lv_obj_add_style( heatingIndicator2, &styleScreenFrame, LV_PART_MAIN );
  lv_obj_clear_flag( heatingIndicator2, LV_OBJ_FLAG_CLICKABLE );
  // bottom bar
  heatingIndicator3 = lv_button_create( lv_scr_act() );
  lv_obj_set_size( heatingIndicator3, LV_HOR_RES, 16 );
  lv_obj_align( heatingIndicator3, LV_ALIGN_BOTTOM_MID, 0, 0 );
  lv_obj_add_style( heatingIndicator3, &styleScreenFrame, LV_PART_MAIN );
  lv_obj_clear_flag( heatingIndicator3, LV_OBJ_FLAG_CLICKABLE );
  // left bar
  heatingIndicator4 = lv_button_create( lv_scr_act() );
  lv_obj_set_size( heatingIndicator4, 16, LV_VER_RES-32 );
  lv_obj_align( heatingIndicator4, LV_ALIGN_LEFT_MID, 0, 0 );
  lv_obj_add_style( heatingIndicator4, &styleScreenFrame, LV_PART_MAIN );
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

  Serial.println("BLINK_FRAME_TOGGLE");

  if( isVisible ){
    lv_style_set_bg_opa( &styleScreenFrame, LV_OPA_30 );
    isVisible = false;
  }
  else {
    lv_style_set_bg_opa( &styleScreenFrame, LV_OPA_TRANSP );
    isVisible = true;
  }

  lv_obj_report_style_change( &styleScreenFrame );
}

static void setDefaultTab( lv_timer_t * timer ) {
  lv_tabview_set_active( tabView, 0, LV_ANIM_OFF );
}

void GUI_Init() {
  xSemaphore = xSemaphoreCreateMutexStatic( &xMutexBuffer );
  assert( xSemaphore );

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

  lv_indev_add_event_cb( indev, touchEventCb, LV_EVENT_SHORT_CLICKED, NULL );

  setScreenMain();

  timer_blinkTimeCurrent = lv_timer_create( blinkTimeCurrent, 250,  NULL );
  lv_timer_pause( timer_blinkTimeCurrent );
  timer_blinkScreenFrame = lv_timer_create( blinkScreenFrame, 500,  NULL );
  lv_timer_pause( timer_blinkScreenFrame );
  timer_setDefaultTab = lv_timer_create( setDefaultTab, 10000,  NULL );
  lv_timer_enable( timer_setDefaultTab );
}

void GUI_Handle( uint32_t tick_period ) {
  if( pdTRUE == xSemaphoreTake( xSemaphore, (TickType_t)( 1000/portTICK_PERIOD_MS ) ) ) {
    lv_timer_handler();
    lv_tick_inc( tick_period );
    xSemaphoreGive( xSemaphore );
  }
}

void GUI_SetTabActive( uint32_t tabNr )
{
  if( inEventHandling
  || pdTRUE == xSemaphoreTake( xSemaphore, (TickType_t)( 1000/portTICK_PERIOD_MS ) ) ) {
    if( (0 > tabNr) || (3 <= tabNr) ) {
      if( 0 == inEventHandling ) {
        xSemaphoreGive( xSemaphore );
      }
      return;
    }

    lv_tabview_set_active( tabView, tabNr, LV_ANIM_OFF );
    if( 0 == inEventHandling ) {
      xSemaphoreGive( xSemaphore );
    }
  }
}

void GUI_SetTargetTemp( uint16_t temp ) {
  if( inEventHandling
  || pdTRUE == xSemaphoreTake( xSemaphore, (TickType_t)( 1000/portTICK_PERIOD_MS ) ) ) {
    char buff[4];
    uint16_t t = temp;
    uint16_t t1, t2, t3;

    rollerTemp = t;

    t1 = (uint16_t)(t / 100);
    t -= ( t1 * 100 );
    t2 = (uint16_t)(t / 10);
    t -= ( t2 * 10 );
    t3 = t;

    buff[0] = ( 0 < t1 ? '0' + t1 : ' ' );
    buff[1] = (( 0 < t2 ) || ( buff[0] != ' ' )) ? '0' + t2 : ' ';  //'0' + t2;
    buff[2] = '0' + t3;
    buff[3] = '\0';

    lv_label_set_text( labelTargetTempVal, buff );
    if( 0 == inEventHandling ) {
      xSemaphoreGive( xSemaphore );
    }
  }
}

void GUI_SetCurrentTemp( uint16_t temp ) {
  if( inEventHandling
  || pdTRUE == xSemaphoreTake( xSemaphore, (TickType_t)( 1000/portTICK_PERIOD_MS ) ) ) {
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
    if( 0 == inEventHandling ) {
      xSemaphoreGive( xSemaphore );
    }
  }
}

void GUI_SetTargetTime( uint32_t time ) {
  if( inEventHandling
  || pdTRUE == xSemaphoreTake( xSemaphore, (TickType_t)( 1000/portTICK_PERIOD_MS ) ) ) {
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
    if( 0 == inEventHandling ) {
      xSemaphoreGive( xSemaphore );
    }
  }
}

void GUI_SetCurrentTime( uint32_t time ) {
  if( inEventHandling
  || pdTRUE == xSemaphoreTake( xSemaphore, (TickType_t)( 1000/portTICK_PERIOD_MS ) ) ) {
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
    if( 0 == inEventHandling ) {
      xSemaphoreGive( xSemaphore );
    }
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

void GUI_setAdjustTimeCallback( adjustTimeCb func ) {
  if( NULL != func ) {
    adjustTimeCB = func;
  }
}

void GUI_setRemoveBakesFromListCallback( removeBakesCb func ) {
  if( NULL != func ) {
    removeBakesCB = func;
  }
}

void GUI_setSwapBakesOnListCallback( swapBakesCb func ) {
  if( NULL != func ) {
    swapBakesCB = func;
  }
}

void GUI_setOperationButtons( enum operationButton btnGroup ) {
  if( inEventHandling
  || pdTRUE == xSemaphoreTake( xSemaphore, (TickType_t)( 1000/portTICK_PERIOD_MS ) ) ) {
    if( BUTTONS_MAX_COUNT > btnGroup ) {
      buttonsGroup = btnGroup;
      createOperatingButtons();
    }
    else {
      buttonsGroup = (buttonsGroup_t)0; // wrong enum received
    }
    if( 0 == inEventHandling ) {
      xSemaphoreGive( xSemaphore );
    }
  }
}

void GUI_setTimeTempChangeAllowed( bool active ) {
  if( inEventHandling
  || pdTRUE == xSemaphoreTake( xSemaphore, (TickType_t)( 1000/portTICK_PERIOD_MS ) ) ) {
    if( active ) {
      lv_obj_add_event_cb( widgetTime, timeEventCb, LV_EVENT_CLICKED, NULL );
      lv_obj_add_event_cb( widgetTemp, tempEventCb, LV_EVENT_CLICKED, NULL );
    }
    else {
      lv_obj_remove_event_cb( widgetTime, timeEventCb );
      lv_obj_remove_event_cb( widgetTemp, tempEventCb );
    }
    if( 0 == inEventHandling ) {
      xSemaphoreGive( xSemaphore );
    }
  }
}

void GUI_setBlinkTimeCurrent( bool active ) {
  if( inEventHandling
  || pdTRUE == xSemaphoreTake( xSemaphore, (TickType_t)( 1000/portTICK_PERIOD_MS ) ) ) {
    if( NULL == timer_blinkTimeCurrent ) {
      if( 0 == inEventHandling ) {
        xSemaphoreGive( xSemaphore );
      }
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
    if( 0 == inEventHandling ) {
      xSemaphoreGive( xSemaphore );
    }
  }
}

void GUI_setBlinkScreenFrame( bool active ) {
  if( inEventHandling
  || pdTRUE == xSemaphoreTake( xSemaphore, (TickType_t)( 1000/portTICK_PERIOD_MS ) ) ) {
    if( NULL == timer_blinkScreenFrame ) {
      if( 0 == inEventHandling ) {
        xSemaphoreGive( xSemaphore );
      }
      return;
    }

    if( active ) {
      lv_timer_resume( timer_blinkScreenFrame );
      Serial.println("BLINK_FRAME_START");
    }
    else {
      lv_timer_pause( timer_blinkScreenFrame );
      lv_style_set_bg_opa( &styleScreenFrame, LV_OPA_TRANSP );
      lv_obj_report_style_change( &styleScreenFrame );
      Serial.println("BLINK_FRAME_STOP");
    }
    if( 0 == inEventHandling ) {
      xSemaphoreGive( xSemaphore );
    }
  }
}

SPIClass * GUI_getSPIinstance() {
  return &(tft.getSPIinstance());
}

void GUI_populateBakeListNames( char *nameList, uint32_t nameLength, uint32_t nameCount ) {
  if( inEventHandling
  || pdTRUE == xSemaphoreTake( xSemaphore, (TickType_t)( 1000/portTICK_PERIOD_MS ) ) ) {
    lv_obj_delete( bakeList );
    bakeList = NULL;  // LVGL bug? pointer is not NULL here
    setContentList( nameList, nameLength, nameCount );
    if( 0 == inEventHandling ) {
      xSemaphoreGive( xSemaphore );
    }
  }
}

void GUI_setTimeBar( uint32_t time ) {
  if( inEventHandling
  || pdTRUE == xSemaphoreTake( xSemaphore, (TickType_t)( 1000/portTICK_PERIOD_MS ) ) ) {
    if( NULL != progressCircle ) {
      lv_arc_set_value( progressCircle, time );
    }
    if( 0 == inEventHandling ) {
      xSemaphoreGive( xSemaphore );
    }
  }
}

void GUI_setTempBar( int32_t temp ) {
  if( inEventHandling
  || pdTRUE == xSemaphoreTake( xSemaphore, (TickType_t)( 1000/portTICK_PERIOD_MS ) ) ) {
    int32_t t = temp;

    if( TERMOMETER_BAR_MAX < t ) {
      t = TERMOMETER_BAR_MAX;
    }
    if( TERMOMETER_BAR_MIN > t ) {
      t = TERMOMETER_BAR_MIN;
    }

    if( NULL != tempBar ) {
      lv_bar_set_value( tempBar, t, LV_ANIM_OFF );
    }
    if( 0 == inEventHandling ) {
      xSemaphoreGive( xSemaphore );
    }
  }
}

void GUI_setBakeName( const char * bakeName ) {
  if( inEventHandling
  || pdTRUE == xSemaphoreTake( xSemaphore, (TickType_t)( 1000/portTICK_PERIOD_MS ) ) ) {
    if( NULL != labelBakeName ) {
      lv_label_set_text( labelBakeName, bakeName );
    }
    if( 0 == inEventHandling ) {
      xSemaphoreGive( xSemaphore );
    }
  }
}

void GUI_setPowerBar( uint32_t power ) {
  if( inEventHandling
  || pdTRUE == xSemaphoreTake( xSemaphore, (TickType_t)( 1000/portTICK_PERIOD_MS ) ) ) {
    if( 100 < power ) {
      power = 100;
    }

    if( NULL != labelPowerBar ) {
      char txt[5];
      sprintf( txt, "%u%%", power );
      lv_label_set_text( labelPowerBar, txt );
    }

    if( NULL != powerBar ) {
      lv_bar_set_value( powerBar, power, LV_ANIM_OFF );
    }
    if( 0 == inEventHandling ) {
      xSemaphoreGive( xSemaphore );
    }
  }
}

void GUI_setPowerIndicator( bool active ) {
  if( inEventHandling
  || pdTRUE == xSemaphoreTake( xSemaphore, (TickType_t)( 1000/portTICK_PERIOD_MS ) ) ) {
    if( NULL != powerBar ) {
      if( active ) {
        lv_obj_set_style_bg_opa( powerBar, LV_OPA_COVER, LV_PART_INDICATOR );
      } else {
        lv_obj_set_style_bg_opa( powerBar, LV_OPA_40, LV_PART_INDICATOR );
      }
    }
    if( 0 == inEventHandling ) {
      xSemaphoreGive( xSemaphore );
    }
  }
}

void GUI_setSoundIcon( bool active ) {
  if( inEventHandling
  || pdTRUE == xSemaphoreTake( xSemaphore, (TickType_t)( 1000/portTICK_PERIOD_MS ) ) ) {
    if( NULL != labelSoundIcon ) {
      if( active ) {
        lv_obj_set_style_text_opa( labelSoundIcon, LV_OPA_COVER, LV_PART_MAIN );
      } else {
        lv_obj_set_style_text_opa( labelSoundIcon, LV_OPA_30, LV_PART_MAIN );
      }
    }
    if( 0 == inEventHandling ) {
      xSemaphoreGive( xSemaphore );
    }
  }
}

void GUI_setWiFiIcon( bool active ) {
  if( inEventHandling
  || pdTRUE == xSemaphoreTake( xSemaphore, (TickType_t)( 1000/portTICK_PERIOD_MS ) ) ) {
    if( NULL != labelWiFiIcon ) {
      if( active ) {
        lv_obj_set_style_text_opa( labelWiFiIcon, LV_OPA_COVER, LV_PART_MAIN );
      } else {
        lv_obj_set_style_text_opa( labelWiFiIcon, LV_OPA_30, LV_PART_MAIN );
      }
    }
    if( 0 == inEventHandling ) {
      xSemaphoreGive( xSemaphore );
    }
  }
}

void GUI_optionsPopulate( setting_t options[], uint32_t cnt ) {
  if( inEventHandling
  || pdTRUE == xSemaphoreTake( xSemaphore, (TickType_t)( 1000/portTICK_PERIOD_MS ) ) ) {
    #define OPTION_HEIGHT 60
    uint32_t i = 1;
    lv_obj_t * label;
    lv_obj_t * labelBtn;

    // first static option: 4 buttons to add/del few minutes to current baking time
    lv_obj_t * widgetOption = lv_button_create( tabOptions );
    lv_obj_set_size( widgetOption, lv_obj_get_style_width( tabOptions, LV_PART_MAIN ), OPTION_HEIGHT );
    lv_obj_set_style_bg_color( widgetOption, lv_palette_darken(LV_PALETTE_GREY, 3), LV_PART_MAIN );
    lv_obj_set_style_bg_opa( widgetOption, LV_OPA_20, LV_PART_MAIN );
    lv_obj_set_style_radius( widgetOption, 0, LV_PART_MAIN );
    lv_obj_set_style_pad_ver( widgetOption, 5, LV_PART_MAIN );
    lv_obj_set_style_pad_hor( widgetOption, 25, LV_PART_MAIN );
    lv_obj_set_style_border_width( widgetOption, 1, LV_PART_MAIN );
    lv_obj_set_style_border_color( widgetOption, lv_palette_darken(LV_PALETTE_GREY, 3), LV_PART_MAIN );// lv_palette_main(LV_PALETTE_BLUE) >> 2196F3
    lv_obj_set_style_border_opa( widgetOption, LV_OPA_40, LV_PART_MAIN );
    lv_obj_set_style_shadow_width( widgetOption, 0, LV_PART_MAIN );
    lv_obj_align( widgetOption, LV_ALIGN_TOP_MID, 0, 0 );
    lv_obj_remove_flag( widgetOption, LV_OBJ_FLAG_PRESS_LOCK );
    lv_obj_remove_flag( widgetOption, LV_OBJ_FLAG_CLICKABLE );

    lv_obj_t * btnDel5m = lv_button_create( widgetOption );
    lv_obj_set_width( btnDel5m, 80 );
    lv_obj_set_style_shadow_width( btnDel5m, 0, LV_PART_MAIN );
    lv_obj_remove_flag( btnDel5m, LV_OBJ_FLAG_PRESS_LOCK );
    lv_obj_add_event_cb( btnDel5m, btnOptionAdjustTimeEventCb, LV_EVENT_CLICKED, (void *)-5 );   // use pointer as ordinary value
    labelBtn = lv_label_create( btnDel5m );
    lv_obj_set_style_text_color( labelBtn, lv_palette_darken(LV_PALETTE_BROWN, 3), LV_PART_MAIN );
    lv_label_set_text( labelBtn, "-5min" );
    lv_obj_center( labelBtn );
    lv_obj_align( btnDel5m, LV_ALIGN_LEFT_MID, -10, 0 );

    lv_obj_t * btnDel1m = lv_button_create( widgetOption );
    lv_obj_set_width( btnDel1m, 80 );
    lv_obj_set_style_shadow_width( btnDel1m, 0, LV_PART_MAIN );
    lv_obj_remove_flag( btnDel1m, LV_OBJ_FLAG_PRESS_LOCK );
    lv_obj_add_event_cb( btnDel1m, btnOptionAdjustTimeEventCb, LV_EVENT_CLICKED, (void *)-1 );   // use pointer as ordinary value
    labelBtn = lv_label_create( btnDel1m );
    lv_obj_set_style_text_color( labelBtn, lv_palette_darken(LV_PALETTE_BROWN, 3), LV_PART_MAIN );
    lv_label_set_text( labelBtn, "-1min" );
    lv_obj_center( labelBtn );
    lv_obj_align( btnDel1m, LV_ALIGN_LEFT_MID, 80, 0 );

    lv_obj_t * btnAdd1m = lv_button_create( widgetOption );
    lv_obj_set_width( btnAdd1m, 80 );
    lv_obj_set_style_shadow_width( btnAdd1m, 0, LV_PART_MAIN );
    lv_obj_remove_flag( btnAdd1m, LV_OBJ_FLAG_PRESS_LOCK );
    lv_obj_add_event_cb( btnAdd1m, btnOptionAdjustTimeEventCb, LV_EVENT_CLICKED, (void *)1 );   // use pointer as ordinary value
    labelBtn = lv_label_create( btnAdd1m );
    lv_obj_set_style_text_color( labelBtn, lv_palette_darken(LV_PALETTE_BROWN, 3), LV_PART_MAIN );
    lv_label_set_text( labelBtn, "+1min" );
    lv_obj_center( labelBtn );
    lv_obj_align( btnAdd1m, LV_ALIGN_RIGHT_MID, -80, 0 );

    lv_obj_t * btnAdd5m = lv_button_create( widgetOption );
    lv_obj_set_width( btnAdd5m, 80 );
    lv_obj_set_style_shadow_width( btnAdd5m, 0, LV_PART_MAIN );
    lv_obj_remove_flag( btnAdd5m, LV_OBJ_FLAG_PRESS_LOCK );
    lv_obj_add_event_cb( btnAdd5m, btnOptionAdjustTimeEventCb, LV_EVENT_CLICKED, (void *)5 );   // use pointer as ordinary value
    labelBtn = lv_label_create( btnAdd5m );
    lv_obj_set_style_text_color( labelBtn, lv_palette_darken(LV_PALETTE_BROWN, 3), LV_PART_MAIN );
    lv_label_set_text( labelBtn, "+5min" );
    lv_obj_center( labelBtn );
    lv_obj_align( btnAdd5m, LV_ALIGN_RIGHT_MID, 10, 0 );

    // dynamically added options
    if( NULL != options && 0 < cnt ) {
      for( int x=0; x<cnt; ++x, i = x+1 ) {
        // clickable option's widget
        widgetOption = lv_button_create( tabOptions );
        lv_obj_set_size( widgetOption, lv_obj_get_style_width( tabOptions, LV_PART_MAIN ), OPTION_HEIGHT );
        lv_obj_set_style_bg_color( widgetOption, lv_palette_darken(LV_PALETTE_GREY, 3), LV_PART_MAIN );
        lv_obj_set_style_bg_opa( widgetOption, LV_OPA_20, LV_PART_MAIN );
        lv_obj_set_style_radius( widgetOption, 0, LV_PART_MAIN );
        lv_obj_set_style_pad_ver( widgetOption, 5, LV_PART_MAIN );
        lv_obj_set_style_pad_hor( widgetOption, 25, LV_PART_MAIN );
        lv_obj_set_style_border_width( widgetOption, 1, LV_PART_MAIN );
        lv_obj_set_style_border_color( widgetOption, lv_palette_darken(LV_PALETTE_GREY, 3), LV_PART_MAIN );// lv_palette_main(LV_PALETTE_BLUE) >> 2196F3
        lv_obj_set_style_border_opa( widgetOption, LV_OPA_40, LV_PART_MAIN );
        lv_obj_set_style_shadow_width( widgetOption, 0, LV_PART_MAIN );
        lv_obj_align( widgetOption, LV_ALIGN_TOP_MID, 0, (x+1) * OPTION_HEIGHT );
        lv_obj_remove_flag( widgetOption, LV_OBJ_FLAG_PRESS_LOCK );
        lv_obj_remove_flag( widgetOption, LV_OBJ_FLAG_CLICKABLE );

        label = lv_label_create( widgetOption );
        lv_label_set_text( label, options[x].name );
        lv_obj_set_style_text_color( label, lv_palette_darken(LV_PALETTE_BROWN, 3), LV_PART_MAIN );
        lv_obj_align( label, LV_ALIGN_LEFT_MID, 0, 0 );
        options[x].btn = lv_button_create( widgetOption );
        lv_obj_set_style_shadow_width( options[x].btn, 0, LV_PART_MAIN );
        lv_obj_set_style_pad_all( options[x].btn, 10, LV_PART_MAIN );
        lv_obj_remove_flag( options[x].btn, LV_OBJ_FLAG_PRESS_LOCK );
        lv_obj_add_event_cb( options[x].btn, btnOptionEventCb, LV_EVENT_CLICKED, &options[x] );
        labelBtn = lv_label_create( options[x].btn );
        lv_obj_set_style_text_color( labelBtn, lv_palette_darken(LV_PALETTE_BROWN, 3), LV_PART_MAIN );
        lv_obj_center( labelBtn );
        lv_obj_align( options[x].btn, LV_ALIGN_RIGHT_MID, 0, 0 );

        switch( options[x].valuetype ) {
          case OPT_VAL_BOOL:
            if( options[x].currentValue.bValue ) {
              lv_label_set_text( labelBtn, "On" );
              lv_obj_set_style_bg_color( options[x].btn, LV_COLOR_MAKE(0x50, 0xAF, 0x4C), LV_PART_MAIN );
            } else {
              lv_label_set_text( labelBtn, "Off" );
              lv_obj_set_style_bg_color( options[x].btn, {0xF0, 0x20, 0x20}, LV_PART_MAIN );
            }
            break;
          case OPT_VAL_INT:
            break;
          case OPT_VAL_TRIGGER:
            lv_label_set_text( labelBtn, "DoIt" );
            break;
        }
      }
    }

    // static option: Removing bakes positions
    widgetOption = lv_button_create( tabOptions );
    lv_obj_set_size( widgetOption, lv_obj_get_style_width( tabOptions, LV_PART_MAIN ), OPTION_HEIGHT );
    lv_obj_set_style_bg_color( widgetOption, lv_palette_darken(LV_PALETTE_GREY, 3), LV_PART_MAIN );
    lv_obj_set_style_bg_opa( widgetOption, LV_OPA_20, LV_PART_MAIN );
    lv_obj_set_style_radius( widgetOption, 0, LV_PART_MAIN );
    lv_obj_set_style_pad_ver( widgetOption, 5, LV_PART_MAIN );
    lv_obj_set_style_pad_hor( widgetOption, 25, LV_PART_MAIN );
    lv_obj_set_style_border_width( widgetOption, 1, LV_PART_MAIN );
    lv_obj_set_style_border_color( widgetOption, lv_palette_darken(LV_PALETTE_GREY, 3), LV_PART_MAIN );
    lv_obj_set_style_border_opa( widgetOption, LV_OPA_40, LV_PART_MAIN );
    lv_obj_set_style_shadow_width( widgetOption, 0, LV_PART_MAIN );
    lv_obj_align( widgetOption, LV_ALIGN_TOP_MID, 0, i++ * OPTION_HEIGHT );
    lv_obj_remove_flag( widgetOption, LV_OBJ_FLAG_PRESS_LOCK );
    lv_obj_remove_flag( widgetOption, LV_OBJ_FLAG_CLICKABLE );

    label = lv_label_create( widgetOption );
    lv_label_set_text( label, "Select bakes to delete" );
    lv_obj_set_style_text_color( label, lv_palette_darken(LV_PALETTE_BROWN, 3), LV_PART_MAIN );
    lv_obj_align( label, LV_ALIGN_LEFT_MID, 0, 0 );
    lv_obj_t * btnRemoveBakes = lv_button_create( widgetOption );
    lv_obj_set_style_shadow_width( btnRemoveBakes, 0, LV_PART_MAIN );
    lv_obj_set_style_pad_all( btnRemoveBakes, 10, LV_PART_MAIN );
    lv_obj_remove_flag( btnRemoveBakes, LV_OBJ_FLAG_PRESS_LOCK );
    lv_obj_remove_flag( btnRemoveBakes, LV_OBJ_FLAG_SCROLL_ON_FOCUS );
    lv_obj_add_event_cb( btnRemoveBakes, btnOptionRemoveBakesEventCb, LV_EVENT_CLICKED, (void *)BAKE_REMOVE );
    labelBtn = lv_label_create( btnRemoveBakes );
    lv_obj_set_style_text_color( labelBtn, lv_palette_darken(LV_PALETTE_BROWN, 3), LV_PART_MAIN );
    lv_label_set_text( labelBtn, "DoIt" );
    lv_obj_center( labelBtn );
    lv_obj_align( btnRemoveBakes, LV_ALIGN_RIGHT_MID, 0, 0 );

    // static option: swaping place of two bakes positions
    widgetOption = lv_button_create( tabOptions );
    lv_obj_set_size( widgetOption, lv_obj_get_style_width( tabOptions, LV_PART_MAIN ), OPTION_HEIGHT );
    lv_obj_set_style_bg_color( widgetOption, lv_palette_darken(LV_PALETTE_GREY, 3), LV_PART_MAIN );
    lv_obj_set_style_bg_opa( widgetOption, LV_OPA_20, LV_PART_MAIN );
    lv_obj_set_style_radius( widgetOption, 0, LV_PART_MAIN );
    lv_obj_set_style_pad_ver( widgetOption, 5, LV_PART_MAIN );
    lv_obj_set_style_pad_hor( widgetOption, 25, LV_PART_MAIN );
    lv_obj_set_style_border_width( widgetOption, 1, LV_PART_MAIN );
    lv_obj_set_style_border_color( widgetOption, lv_palette_darken(LV_PALETTE_GREY, 3), LV_PART_MAIN );
    lv_obj_set_style_border_opa( widgetOption, LV_OPA_40, LV_PART_MAIN );
    lv_obj_set_style_shadow_width( widgetOption, 0, LV_PART_MAIN );
    lv_obj_align( widgetOption, LV_ALIGN_TOP_MID, 0, i++ * OPTION_HEIGHT );
    lv_obj_remove_flag( widgetOption, LV_OBJ_FLAG_PRESS_LOCK );
    lv_obj_remove_flag( widgetOption, LV_OBJ_FLAG_CLICKABLE );

    label = lv_label_create( widgetOption );
    lv_label_set_text( label, "Swap 2 bake positions" );
    lv_obj_set_style_text_color( label, lv_palette_darken(LV_PALETTE_BROWN, 3), LV_PART_MAIN );
    lv_obj_align( label, LV_ALIGN_LEFT_MID, 0, 0 );
    lv_obj_t * btnSwapBakes = lv_button_create( widgetOption );
    lv_obj_set_style_shadow_width( btnSwapBakes, 0, LV_PART_MAIN );
    lv_obj_set_style_pad_all( btnSwapBakes, 10, LV_PART_MAIN );
    lv_obj_remove_flag( btnSwapBakes, LV_OBJ_FLAG_PRESS_LOCK );
    lv_obj_remove_flag( btnSwapBakes, LV_OBJ_FLAG_SCROLL_ON_FOCUS );
    lv_obj_add_event_cb( btnSwapBakes, btnOptionRemoveBakesEventCb, LV_EVENT_CLICKED, (void *)BAKE_SWAP );  // use same callback as for removal
    labelBtn = lv_label_create( btnSwapBakes );
    lv_obj_set_style_text_color( labelBtn, lv_palette_darken(LV_PALETTE_BROWN, 3), LV_PART_MAIN );
    lv_label_set_text( labelBtn, "DoIt" );
    lv_obj_center( labelBtn );
    lv_obj_align( btnSwapBakes, LV_ALIGN_RIGHT_MID, 0, 0 );

    if( 0 == inEventHandling ) {
      xSemaphoreGive( xSemaphore );
    }
  }
}

void GUI_updateOption( setting_t &option ) {
  if( inEventHandling
  || pdTRUE == xSemaphoreTake( xSemaphore, (TickType_t)( 1000/portTICK_PERIOD_MS ) ) ) {
    switch( option.valuetype ) {
      case OPT_VAL_BOOL:
        if( option.btn ) {
          lv_obj_t * labelBtn = lv_obj_get_child( option.btn, 0 );  // first child is the label in our case
          lv_obj_set_style_text_color( labelBtn, lv_palette_darken(LV_PALETTE_BROWN, 3), LV_PART_MAIN );
          lv_obj_center( labelBtn );
          if( option.currentValue.bValue ) {
            lv_label_set_text( labelBtn, "On" );
            lv_obj_set_style_bg_color( option.btn, LV_COLOR_MAKE(0x50, 0xAF, 0x4C), LV_PART_MAIN );
          } else {
            lv_label_set_text( labelBtn, "Off" );
            lv_obj_set_style_bg_color( option.btn, {0xF0, 0x20, 0x20}, LV_PART_MAIN );
          }
        }
        break;
      case OPT_VAL_INT:
        break;
      case OPT_VAL_TRIGGER:
        // nothing to do (button is the same)
        break;
    }
    if( 0 == inEventHandling ) {
      xSemaphoreGive( xSemaphore );
    }
  }
}
