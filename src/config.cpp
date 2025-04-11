#include "config.h"
#include "sdcard.h"
#include "helper.h"
#include "ArduinoJson.h"
#include "EEPROM.h"
#include "esp_err.h"
#include "esp_spiffs.h"
#include <sys/stat.h>

#define EEPROM_SIZE     1024  // 1kB from eeprom(flash) used

typedef struct option
{
  int val;
  String fileName;
} option_t;

static bool configAvailable = false;
static bake_t * bakeList = NULL;          //dynamically allocated buffer
static uint32_t bakesCount = 0;
static bool spiffsMounted = false;
static esp_vfs_spiffs_conf_t conf = {
  .base_path = "/spiffs",
  .partition_label = NULL,
  .max_files = 5,
  .format_if_mount_failed = true
};

/**
 * Set few bake positions and save them to file on SDCARD
 */
static void setBakeExample() {
  JsonDocument doc;

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

static void loadBakesFromSDCard() {
  uint8_t * buffer = NULL;
  uint32_t rlen;
  bake_t * tmpBakeList;
  uint32_t tmpBakesCount;
  JsonDocument doc;

  rlen = SDCARD_getFileContent( BAKE_FILE_NAME, &buffer );
  if( NULL != buffer ) {
    buffer[ rlen ] = '\0';
    // Serial.printf( "buffer[%d]: %s\n", rlen, buffer );

    deserializeJson( doc, buffer );
    tmpBakesCount = doc["count"];

    if( 0 < tmpBakesCount ) {
      Serial.printf( "%d new positions will be added to current bake list\n", tmpBakesCount );
      tmpBakesCount += bakesCount;
    } else {
      Serial.println( "File doesn't contain proper data!" );
      free( buffer );
      return;
    }

    // allocate memory for tmpBakeList (including size of current bakeList)
    tmpBakeList = (bake_t *)malloc( sizeof( bake_t ) * tmpBakesCount );
    if( NULL == tmpBakeList ) {
      Serial.println( "CONF(loadBakesFromSDCard): malloc failed!" );
      free( buffer );
      return;
    }

    // copy old list positions to the newly created list
    memcpy( tmpBakeList, bakeList, sizeof( bake_t ) * bakesCount );

    // add new positions at the end of new list
    for( int i = bakesCount, x = 0; i < tmpBakesCount; i++, x++ ) {
      strlcpy( tmpBakeList[i].name, doc["data"][x]["name"], sizeof( tmpBakeList[i].name ) );
      tmpBakeList[i].stepCount = doc["data"][x]["stepCount"];
      for( int s = 0; s < tmpBakeList[i].stepCount; s++ ) {
        tmpBakeList[i].step[s].temp = doc["data"][x]["step"][s]["temp"];
        tmpBakeList[i].step[s].time = doc["data"][x]["step"][s]["time"];
      }
    }

    free( buffer );
    free( bakeList );       // delete old bake list
    bakeList = tmpBakeList; // remember new list
    bakesCount = tmpBakesCount;
  }
}

static void spiffsMount() {
  if( spiffsMounted ) {
    return;
  }

  // Initialize and mount SPIFFS filesystem
  esp_err_t ret = esp_vfs_spiffs_register( &conf );

  if ( ESP_OK != ret ) {
    if ( ESP_FAIL == ret ) {
      Serial.printf( "CONF(spiffsMount): Failed to mount filesystem\n" );
    } else if ( ESP_ERR_NOT_FOUND == ret ) {
      Serial.printf( "CONF(spiffsMount): Failed to find SPIFFS partition\n" );
    } else {
      Serial.printf( "CONF(spiffsMount): Failed to initialize SPIFFS (%s)\n", esp_err_to_name(ret) );
    }
    return;
  }

  size_t total = 0, used = 0;
  ret = esp_spiffs_info( conf.partition_label, &total, &used );
  if ( ESP_OK != ret ) {
    Serial.printf( "CONF(spiffsMount): Failed to get SPIFFS partition information (%s). Formatting...\n", esp_err_to_name(ret) );
    esp_spiffs_format( conf.partition_label );
    return;
  } else {
    Serial.printf( "Partition size: total: %d, used: %d\n", total, used );
  }

  // Check consistency of reported partition size info
  if ( used > total ) {
    Serial.printf( "Number of used bytes cannot be larger than total. Performing SPIFFS_check()...\n" );
    ret = esp_spiffs_check( conf.partition_label );
    if ( ESP_OK != ret ) {
      Serial.printf( "CONF(spiffsMount): SPIFFS_check() failed (%s)\n", esp_err_to_name(ret) );
      return;
    } else {
      Serial.printf( "SPIFFS_check() successful\n" );
    }
  }

  spiffsMounted = true;
}

static void spiffsUnmount() {
  if( !spiffsMounted ) {
    return;
  }

  // unmount partition and disable SPIFFS
  esp_vfs_spiffs_unregister( conf.partition_label );
  Serial.printf( "SPIFFS partition unmounted\n" );

  spiffsMounted = false;
}

static void loadBakesFromFlash() {
  uint32_t fileSize = 0;

  Serial.printf( "Loading bakes from file (flash)...\n" );

  spiffsMount();
  if( !spiffsMounted ) {
    return;
  }

  // Check destination file size
  struct stat st;
  if ( 0 != stat( "/spiffs/bakes.txt", &st ) ) {
    Serial.printf( "File 'bakes.txt' doesn't exist\n" );
    return;
  }
  fileSize = (uint32_t)st.st_size;

  char * buff;
  buff = (char *)malloc( fileSize );

  if( NULL == buff ) {
    Serial.printf( "CONF(loadBakesFromFlash) Malloc failed for buff\n" );
    return;
  }

  FILE * f = fopen( "/spiffs/bakes.txt", "r" );
  if ( NULL == f ) {
    Serial.printf( "CONF(loadBakesFromFlash) Failed to open file for reading\n" );
    free( buff );
    return;
  }

  uint32_t readSize;
  readSize = (uint32_t)fread( buff, sizeof(char), fileSize, f );
  Serial.printf( "fileSize:%d, readSize:%d\n", fileSize, readSize );
  fclose( f );

  JsonDocument doc;

  deserializeJson( doc, buff );
  free( buff );
  bakesCount = doc["count"];

  // allocate memory for bakeList
  bakeList = (bake_t *)calloc( bakesCount, sizeof( bake_t ) );
  if( NULL == bakeList ) {
    Serial.printf( "CONF(loadBakesFromFlash) Malloc failed for bakeList\n" );
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

  spiffsUnmount();
}

void CONF_Init( SPIClass * spi ) {
  SDCARD_Setup( spi );

  configAvailable = true;

  // setBakeExample();
  loadBakesFromFlash();
  // loadBakesFromSDCard();
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

int32_t CONF_getBakeTime( uint32_t idx, uint32_t step ) {
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

  if( NULL == list || 0 == count ) {
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

bool CONF_swapBakes( uint8_t list[] ) {
  bake_t tmpBake;

  if( NULL == list ) {
    return false;
  }

  memcpy( &tmpBake, &bakeList[ list[0] ], sizeof( bake_t ) );
  memcpy( &bakeList[ list[0] ], &bakeList[ list[1] ], sizeof( bake_t ) );
  memcpy( &bakeList[ list[1] ], &tmpBake, sizeof( bake_t ) );

  return true;
}

void CONF_addBakesFromFile( void ) {
  if( SDCARD_Reinit() ) {
    loadBakesFromSDCard();
    SDCARD_Eject();
  } else {
    Serial.println( "CONF(addBakesFromFile): SDCarc initialization failed" );
  }
}

void CONF_storeBakeList() {
  spiffsMount();

  if( !spiffsMounted ) {
    return;
  }

  JsonDocument doc;

  // prepare file content
  doc["count"] = bakesCount;
  for( int x=0; x<bakesCount; x++ ) {
    doc["data"][x]["name"] = bakeList[x].name;
    doc["data"][x]["stepCount"] = bakeList[x].stepCount;
    for( int y=0; y<bakeList[x].stepCount; y++ ) {
      doc["data"][x]["step"][y]["temp"] = bakeList[x].step[y].temp;
      doc["data"][x]["step"][y]["time"] = bakeList[x].step[y].time;
    }
  }

  String output;
  serializeJson( doc, output );
  Serial.printf( "Data to be saved: %s\n", output.c_str() );

  // save file to partition
  Serial.printf( "Opening file...\n" );
  FILE * f = fopen( "/spiffs/bakes.txt", "w" );
  if ( NULL == f ) {
    Serial.printf( "CONF(storeBakeList): Failed to open file for writing\n" );
    return;
  }
  fprintf( f, "%s", output.c_str() );
  fclose( f );
  Serial.printf( "File written\n" );

  spiffsUnmount();
}
