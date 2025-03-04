#include "config.h"
#include "sdcard.h"
#include "helper.h"
#include "ArduinoJson.h"

typedef struct option
{
  int val;
  String fileName;
} option_t;

static bool configAvailable = false;
static option_t options[ OPTIONS_COUNT ];
static bake_t * bakeList = NULL;          //dynamically allocated buffer
static uint32_t bakesCount = 0;
static JsonDocument doc;

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

static void setBakesDefault() {
  // bakeList[0].name = "Wypiek #1";
  // bakeList[0].temp = 100;
  // bakeList[0].time = HOURS_TO_SECONDS( 1 );
  // bakeList[1].name = "Wypiek #2";
  // bakeList[1].temp = 150;
  // bakeList[1].time = HOURS_TO_SECONDS( 1 );
  // bakeList[2].name = "Wypiek #3";
  // bakeList[2].temp = 200;
  // bakeList[2].time = HOURS_TO_SECONDS( 1 );
  // bakeList[3].name = "Wypiek #4";
  // bakeList[3].temp = 150;
  // bakeList[3].time = HOURS_TO_SECONDS( 1 );
  // bakeList[4].name = "Wypiek #5";
  // bakeList[4].temp = 200;
  // bakeList[4].time = HOURS_TO_SECONDS( 1 );

  doc["data"][0]["name"] = "Wypiek #1";
  doc["data"][0]["temp"] = 101;
  doc["data"][0]["time"] = HOURS_TO_SECONDS( 1 );
  doc["data"][1]["name"] = "Wypiek #2";
  doc["data"][1]["temp"] = 102;
  doc["data"][1]["time"] = HOURS_TO_SECONDS( 1 );
  doc["data"][2]["name"] = "Wypiek #3";
  doc["data"][2]["temp"] = 103;
  doc["data"][2]["time"] = HOURS_TO_SECONDS( 1 );
  doc["data"][3]["name"] = "Wypiek #4";
  doc["data"][3]["temp"] = 104;
  doc["data"][3]["time"] = HOURS_TO_SECONDS( 1 );
  doc["data"][4]["name"] = "Wypiek #5";
  doc["data"][4]["temp"] = 105;
  doc["data"][4]["time"] = HOURS_TO_SECONDS( 1 );

  String output;
  serializeJson( doc, output );
  Serial.println( output );
  SDCARD_writeFile( "/bakes.txt", output.c_str() );
}

static void loadBakesFromFile() {
  uint8_t * buffer = NULL;
  uint32_t rlen;

  rlen = SDCARD_getFileContent( BAKE_FILE_NAME, &buffer );
  if( NULL != buffer ) {
    buffer[ rlen ] = '\0';
    // Serial.printf( "buffer[%d]: %s\n", rlen, buffer );

    deserializeJson( doc, buffer );
    bakesCount = doc["count"];

    // allocate memory for bakeList
    bakeList = (bake_t *)malloc( sizeof( bake_t ) * bakesCount );
    if( NULL == bakeList ) {
      free( buffer );
      return;
    }

    for( int i = 0; i < bakesCount; i++ ) {
      strlcpy( bakeList[i].name, doc["data"][i]["name"], sizeof( bakeList[i].name ) );
      bakeList[i].temp = doc["data"][i]["temp"];
      bakeList[i].time = doc["data"][i]["time"];
    }

    free( buffer );
  }

  Serial.println( "bakeList:" );
  for( int i = 0; i < bakesCount; i++ ) {
    Serial.printf( "[%d]Name:%s;Temp:%d;Time:%d\n", i, bakeList[i].name, bakeList[i].temp, bakeList[i].time );
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

  // setBakesDefault();
  loadBakesFromFile();
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

void CONF_getBakeNames( bakeName **bList, uint32_t *cnt ) {
  bakeName * bakeNames;

  if( NULL == bakeList ) {
    return;
  }

  bakeNames = (bakeName *)malloc( sizeof(bakeName) * bakesCount );
  if( NULL == bakeNames ) {
    *bList = NULL;
    *cnt = 0;
    Serial.println( "CONF(getBakeNames): malloc failed" );
    return;
  }

  *cnt = bakesCount;
  *bList = bakeNames;
  for( int x=0; x<bakesCount; x++) {
    snprintf( (char *)(*bList+x), sizeof(bakeName), "%s", bakeList[x].name );
  }
}

uint32_t CONF_getBakeTemp( uint32_t idx ) {
  if( NULL == bakeList || bakesCount <= idx ) {
    return 0;
  }
  return bakeList[ idx ].temp;
}

uint32_t CONF_getBakeTime( uint32_t idx ) {
  if( NULL == bakeList || bakesCount <= idx ) {
    return 0;
  }
  return bakeList[ idx ].time;
}

char * CONF_getBakeName( uint32_t idx ) {
  if( NULL == bakeList || bakesCount <= idx ) {
    return 0;
  }
  return bakeList[ idx ].name;
}

void CONF_reloadBakeFile() {
  if( SDCARD_Reinit() ) {
    // clear old data
    bakesCount = 0;
    free( bakeList );

    loadBakesFromFile();
  }
}
