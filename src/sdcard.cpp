#include "sdcard.h"

static bool cardAvailable = false;

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

void SDCARD_Setup() {
  // Initialize SD card
  SD.begin( SD_CS );  

  if( !SD.begin( SD_CS ) ) {
    Serial.println( "Card Mount Failed" );
    return;
  }

  uint8_t cardType = SD.cardType();

  if( cardType == CARD_NONE ) {
    Serial.println( "No SD card attached" );
    return;
  }

  cardAvailable = true;

  // If the data.txt file doesn't exist
  // Create a file on the SD card and write the data labels
  File file = SD.open( "/data.txt" );

  if( !file ) {
    Serial.println( "File doens't exist" );
    Serial.println( "Creating file..." );
    writeFile( SD, "/data.txt", "Reading ID, Date, Hour, Temperature \r\n" );
  }
  else {
    Serial.println("File already exists");  
  }
  file.close();
}

void SDCARD_log() {
  String dataMessage = String(millis()) + "," + String(34) + "," + String(56) + "," + String(78) + "\r\n";
  // Serial.print( "Save data: " );
  // Serial.println( dataMessage );
  appendFile( SD, "/data.txt", dataMessage.c_str() );
}
