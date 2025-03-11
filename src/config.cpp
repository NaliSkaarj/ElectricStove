#include "config.h"
#include "sdcard.h"
#include "helper.h"
#include "ArduinoJson.h"
#include "EEPROM.h"

#define EEPROM_SIZE     1024  // 1kB from eeprom(flash) used

typedef struct option
{
  int val;
  String fileName;
} option_t;

static bool configAvailable = false;
static bake_t * bakeList = NULL;          //dynamically allocated buffer
static uint32_t bakesCount = 0;
static JsonDocument doc;

/**
 * Set few bake positions and save them to file on SDCARD
 */
static void setBakeExample() {
  doc["count"] = 2;

  doc["data"][0]["name"] = "Wypiek #1";
  doc["data"][0]["stepCount"] = 1;
  doc["data"][0]["step"][0]["temp"] = 101;
  doc["data"][0]["step"][0]["time"] = HOURS_TO_SECONDS( 1 );

  doc["data"][1]["name"] = "Wypiek #2";
  doc["data"][1]["stepCount"] = 2;
  doc["data"][1]["step"][0]["temp"] = 50;
  doc["data"][1]["step"][0]["time"] = MINUTES_TO_SECONDS( 10 );
  doc["data"][1]["step"][1]["temp"] = 100;
  doc["data"][1]["step"][1]["time"] = MINUTES_TO_SECONDS( 20 );

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
      bakeList[i].stepCount = doc["data"][i]["stepCount"];
      for( int s = 0; s < bakeList[i].stepCount; s++ ) {
        bakeList[i].step[s].temp = doc["data"][i]["step"][s]["temp"];
        bakeList[i].step[s].time = doc["data"][i]["step"][s]["time"];
      }
    }

    free( buffer );
  }
}

void CONF_Init( SPIClass * spi ) {
  SDCARD_Setup( spi );

  configAvailable = true;

  // setBakeExample();
  loadBakesFromFile();
}

int CONF_getOptionInt( int32_t option ) {
  if( false == configAvailable ) {
    return -1;
  }

  int32_t retVal = 0;

  if( EEPROM.begin( EEPROM_SIZE ) ) {
    retVal = EEPROM.readInt( option );
    EEPROM.end();
  }

  return retVal;
}

bool CONF_getOptionBool( int32_t option ) {
  if( false == configAvailable ) {
    return false;
  }

  bool retVal = false;

  if( EEPROM.begin( EEPROM_SIZE ) ) {
    retVal = EEPROM.readBool( option );
    EEPROM.end();
  }

  return retVal;
}

void CONF_setOptionBool( int32_t option, bool value ) {
  if( false == configAvailable ) {
    return;
  }

  if( EEPROM.begin( EEPROM_SIZE ) ) {
    EEPROM.writeInt( option, value );
    EEPROM.commit();
    EEPROM.end();
  }
}

void CONF_setOptionInt( int32_t option, int32_t value ) {
  if( false == configAvailable ) {
    return;
  }

  if( EEPROM.begin( EEPROM_SIZE ) ) {
    EEPROM.writeBool( option, value );
    EEPROM.commit();
    EEPROM.end();
  }
}

void CONF_getBakeNames( bakeName **bList, uint32_t *cnt ) {
  bakeName * bakeNames;

  if( NULL == bakeList ) {
    *bList = NULL;
    *cnt = 0;
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

uint32_t CONF_getBakeTemp( uint32_t idx, uint32_t step ) {
  if( NULL == bakeList || bakesCount <= idx || BAKE_MAX_STEPS <= step ) {
    return 0;
  }
  return bakeList[ idx ].step[ step ].temp;
}

uint32_t CONF_getBakeTime( uint32_t idx, uint32_t step ) {
  if( NULL == bakeList || bakesCount <= idx || BAKE_MAX_STEPS <= step ) {
    return 0;
  }
  return bakeList[ idx ].step[ step ].time;
}

char * CONF_getBakeName( uint32_t idx ) {
  if( NULL == bakeList || bakesCount <= idx ) {
    return 0;
  }
  return bakeList[ idx ].name;
}

bool CONF_removeBakes( uint8_t list[], uint32_t count ) {
  bool retVal = false;

  if( 0 == count ) {
    return false;
  }

  for( int x=0; x<count; x++ ) {
    bakeList[ list[x] ].stepCount = 0;    // use stepCount as marker (mark this element as not active)
  }

  // move last elements to previously removed places
  for( int x=0; x<bakesCount; x++ ) {
    if( 0 == bakeList[x].stepCount ) {  // found empty slot
      for( int src=x, dst=x+1; dst<bakesCount; src++, dst++ ) {
        memcpy( &bakeList[src], &bakeList[dst], sizeof( bake_t ) );
      }
      bakesCount--;
      // mark last element as not active
      bakeList[ bakesCount ].stepCount = 0;
      retVal = true;
      x--; // don't skip 'next first' element after movement of the rest
    }
  }

  return retVal;
}

void CONF_reloadBakeFile() {
  if( SDCARD_Reinit() ) {
    // clear old data
    bakesCount = 0;
    free( bakeList );

    loadBakesFromFile();
  }
}
