#include "gui.h"
#include "TFT_eSPI.h"
#include "lvgl.h"

static lv_obj_t * tabView;    // main container for 3 tabs
static lv_style_t styleTabs;  // has impact on tabs icons size
static lv_obj_t * tabHome;    // the widget where the content of the tab HOME can be created
static lv_obj_t * tabList;    // the widget where the content of the tab LIST can be created
static lv_obj_t * tabOptions; // the widget where the content of the tab OPTIONS can be created

TFT_eSPI tft = TFT_eSPI();
static lv_color_t buf[LV_HOR_RES_MAX * LV_VER_RES_MAX / 10]; // Declare a buffer for 1/10 screen size

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

static void setContentHome() {
  static lv_style_t styleTabHome;

  lv_obj_set_style_bg_color( tabHome, {0xff, 0x00, 0x00}, 0 ); // red
  lv_obj_set_style_bg_opa( tabHome, LV_OPA_COVER, 0 );

  /*Add content to the tabs*/
  lv_obj_t * label = lv_label_create( tabHome );
  lv_label_set_text( label, "First tab" );

  // font size & etc
  lv_style_init( &styleTabHome );
  lv_style_set_text_decor( &styleTabHome, LV_TEXT_DECOR_NONE );
  lv_style_set_text_font( &styleTabHome, &lv_font_montserrat_16 );
  lv_obj_add_style( tabHome, &styleTabHome, 0 );
}

static void setContentList() {
  static lv_style_t styleTabList;

  lv_obj_set_style_bg_color( tabList, {0x00, 0xff, 0x00}, 0 ); // green
  lv_obj_set_style_bg_opa( tabList, LV_OPA_COVER, 0 );

  /*Add content to the tabs*/
  lv_obj_t * label = lv_label_create( tabList );
  lv_label_set_text( label, "Second tab (0F0 green)" );

  // font size & etc
  lv_style_init( &styleTabList );
  lv_style_set_text_decor( &styleTabList, LV_TEXT_DECOR_NONE );
  lv_style_set_text_font( &styleTabList, &lv_font_montserrat_32 );
  lv_obj_add_style( tabList, &styleTabList, 0 );
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
  lv_style_set_text_font( &styleTabOptions, &lv_font_montserrat_24 );
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

  setContentHome();
  setContentList();
  setContentOptions();
}

void GUI_Init() {
    
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

  setScreenMain();
}

void GUI_Handle( uint32_t tick_period ) {
  lv_timer_handler();
  lv_tick_inc( 10 );
}

void GUI_SetTabActive( uint32_t tabNr )
{
  lv_tabview_set_active( tabView, tabNr, LV_ANIM_OFF );
  Serial.println( "Timer callback function executed" );
}
