#include "config.h"
#include "sdcard.h"

typedef struct option
{
  int val;
  String fileName;
} option_t;

static bool configAvailable = false;
static option_t options[ OPTIONS_COUNT ];

static void loadConfiguration() {
  // TODO-later_maybe: read file config.ini and set local options variable
  int val;

  for( int x=0; x<OPTIONS_COUNT; x++ ) {
    val = SDCARD_readFile( options[x].fileName.c_str() );
    if( 0 <= val ) {
      options[x].val = val;
    }
  }
}

void CONF_Init( SPIClass * spi ) {
  for( int x=0; x<OPTIONS_COUNT; x++ ) {
    options[ x ].val = 0;
    options[ x ].fileName = (String)"/file" + (String)x;
  }

  SDCARD_Setup( spi );

  loadConfiguration();
  configAvailable = true;
}

int CONF_getOption( enum options option ) {
  if( false == configAvailable ) {
    return -1;
  }

  return options[ option ].val;
}

void CONF_setOption( enum options option, int val ) {
  if( false == configAvailable ) {
    return;
  }

  options[ option ].val = val;
  SDCARD_writeFile( options[ option ].fileName.c_str(), options[ option ].val );
}
