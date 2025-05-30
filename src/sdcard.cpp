#include "sdcard.h"

static bool cardAvailable = false;
static SPIClass * sharedSPI;

static void writeFile( fs::FS &fs, const char * path, const char * message ) {
  if( false == cardAvailable ) {
    Serial.println( "SDCARD(write): No SDCard" );
    return;
  }

  File file = fs.open( path, FILE_WRITE );

  if( !file ) {
    Serial.println( "SDCARD(write): Failed to open file" );
    return;
  }

  if( file.print( message ) ) {
    Serial.println( "SDCARD(write): Write success" );
  } else {
    Serial.println( "SDCARD(write): Write failed" );
  }

  file.close();
}

static void appendFile( fs::FS &fs, const char * path, const char * message ) {
  if( false == cardAvailable ) {
    Serial.println( "SDCARD(append): No SDCard" );
    return;
  }

  File file = fs.open( path, FILE_APPEND );

  if( !file ) {
    Serial.println( "SDCARD(append): Failed to open file" );
    return;
  }

  if( file.print( message ) ) {
    Serial.println( "SDCARD(append): append success" );
  } else {
    Serial.println( "SDCARD(append): append failed" );
  }

  file.close();
}

static void printDirectory( File dir, int numTabs ) {
  while ( true ) {
    File entry = dir.openNextFile();
    if ( !entry ) {
      // no more files
      break;
    }
    for ( uint8_t i = 0; i < numTabs; i++ ) {
      Serial.print( '\t' );
    }
    Serial.print( entry.name() );
    if ( entry.isDirectory() ) {
      Serial.println( "/" );
      printDirectory( entry, numTabs + 1 );
    } else {
      Serial.print( "\t\t" );
      Serial.println( entry.size(), DEC );
    }
    entry.close();
  }
}

static bool initializeSdCard() {
  if( !SD.begin( SD_CS, *sharedSPI ) ) {
    Serial.println( "Card Mount Failed" );
    cardAvailable = false;
    return false;
  }

  uint8_t cardType = SD.cardType();

  if( cardType == CARD_NONE ) {
    Serial.println( "No SD card attached" );
    cardAvailable = false;
    return false;
  }

  Serial.print("SD Card Type: ");
  if (cardType == CARD_MMC) {
    Serial.println("MMC");
  } else if (cardType == CARD_SD) {
    Serial.println("SDSC");
  } else if (cardType == CARD_SDHC) {
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);

  cardAvailable = true;

  return true;
}

void SDCARD_Setup( SPIClass * spi ) {
  if( NULL == spi ) {
    return;
  }

  sharedSPI = spi;

  if( false == initializeSdCard() ) {
    return;
  }
}

bool SDCARD_Reinit() {
  SD.end();
  SDCARD_Setup( sharedSPI );
  return cardAvailable;
}

void SDCARD_Eject() {
  SD.end();
}

void SDCARD_list() {
  if( !cardAvailable ) {
    return;
  }

  File root = SD.open( "/" );
  if( root ) {
    unsigned long start = micros();
    printDirectory( root, 0 );
    Serial.printf( "listing time: %llu[uS]\n", micros() - start );
    root.close();
  }
}

int SDCARD_readFile( const char * path ) {
  int retVal = 0;

  if( false == cardAvailable ) {
    Serial.println( "SDCARD(readFile): No SDCard" );
    return -1;
  }

  File file = SD.open( path, FILE_READ );

  if( !file ) {
    Serial.println( "SDCARD(readFile): Failed to open file, creating file" );
    writeFile( SD, path, "-1" );
    return -1;
  }

  retVal = file.read();
  file.close();

  return retVal;
}

void SDCARD_writeFile( const char * path, int value ) {
  if( false == cardAvailable ) {
    Serial.println( "SDCARD(writeFile): No SDCard" );
    return;
  }

  File file = SD.open( path, FILE_WRITE );

  if( !file ) {
    Serial.println( "SDCARD(writeFile): Failed to open file, creating file" );
    writeFile( SD, path, String(value).c_str() );
    return;
  }

  if( file.print( value ) ) {
    Serial.println( "SDCARD(writeFile): Write success" );
  } else {
    Serial.println( "SDCARD(writeFile): Write failed" );
  }

  file.close();
}

void SDCARD_writeFile( const char * path, const char * msg ) {
  writeFile( SD, path, msg );
}

uint32_t SDCARD_getFileContent( const char * path, uint8_t ** buf ) {
  uint32_t retVal = 0;

  if( false == cardAvailable ) {
    Serial.println( "SDCARD(getFileContent): No SDCard" );
    return 0;
  }

  if( NULL == path ) {
    return retVal;
  }

  File file = SD.open( path, FILE_READ );

  if( file ) {
    uint32_t bufLen = (uint32_t)file.size() + 1;  // +1 for safety (we can end buffer content with NULL character)

    if( 1 < bufLen ) {
      *buf = (uint8_t *)malloc( bufLen );
    }
    // Serial.printf( "Allocated buffer size: %d\n", bufLen );
    // Serial.printf( "file content: %s\n", *buf );

    if( NULL != *buf ) {
      int rlen = file.available();
      // Serial.printf( "file available: %d\n", rlen );

      if( 0 < rlen && rlen < bufLen ) {
        retVal = (uint32_t)( file.read( *buf, bufLen ) );
      }
      else {
        Serial.println( "SDCARD(readFileContent): Buffer size to small" );
        free( *buf );
        *buf = NULL;
      }
    }
    file.close();
  }

  return retVal;
}