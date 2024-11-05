#include "gui.h"
#include "TFT_eSPI.h"
#include "lvgl.h"

lv_obj_t * slider_label;
lv_obj_t * tabview;
static lv_style_t styleTabs;

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

static void setContentHome( lv_obj_t *tab ) {
  lv_obj_set_style_bg_color( tab, {0xff, 0x00, 0x00}, 0 ); // red
  lv_obj_set_style_bg_opa( tab, LV_OPA_COVER, 0 );

  /*Add content to the tabs*/
  lv_obj_t * label = lv_label_create( tab );
  lv_label_set_text( label, "First tab" );
}

static void setContentList( lv_obj_t *tab ) {
  lv_obj_set_style_bg_color( tab, {0x00, 0xff, 0x00}, 0 ); // green
  lv_obj_set_style_bg_opa( tab, LV_OPA_COVER, 0 );

  /*Add content to the tabs*/
  lv_obj_t * label = lv_label_create( tab );
  lv_label_set_text( label, "Second tab (0F0 green)" );
}

static void setContentOptions( lv_obj_t *tab ) {
  lv_obj_set_style_bg_color( tab, {0x00, 0x00, 0xff}, 0 ); // blue
  lv_obj_set_style_bg_opa( tab, LV_OPA_COVER, 0 );

  /*Add content to the tabs*/
  lv_obj_t * label = lv_label_create( tab );
  lv_label_set_text( label, "Third tab (00F blue)" );
}

static void setScreenMain() {
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
  lv_obj_t * tabHome = lv_tabview_add_tab( tabview, LV_SYMBOL_HOME );
  lv_obj_t * tabList = lv_tabview_add_tab( tabview, LV_SYMBOL_LIST );
  lv_obj_t * tabOptions = lv_tabview_add_tab( tabview, LV_SYMBOL_SETTINGS );

  lv_style_init( &styleTabs );
  lv_style_set_text_decor( &styleTabs, LV_TEXT_DECOR_NONE );
  lv_style_set_text_font( &styleTabs, &lv_font_montserrat_32 );
  lv_obj_add_style( tabHome, &styleTabs, 0 );
  lv_obj_add_style( tabList, &styleTabs, 0 );
  lv_obj_add_style( tabOptions, &styleTabs, 0 );

  setContentHome( tabHome );
  setContentList( tabList );
  setContentOptions( tabOptions );

  lv_obj_remove_flag( lv_tabview_get_content(tabview), LV_OBJ_FLAG_SCROLLABLE );
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
  lv_tabview_set_active( tabview, tabNr, LV_ANIM_OFF );
  Serial.println( "Timer callback function executed" );
}
