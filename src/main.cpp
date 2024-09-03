#include <Arduino.h>
#include "TFT_eSPI.h"

uint16_t x, y;
bool color = true;
unsigned long currentTime, nextTime;

TFT_eSPI tft = TFT_eSPI();

void setup() {
  Serial.begin( 115200 );
  tft.init();
  tft.setRotation( 3 );

  uint16_t calData[5] = { 265, 3677, 261, 3552, 1 };
  tft.setTouch(calData);

  currentTime = nextTime = millis();
}

void loop() {
  currentTime = millis();
  // tft.getTouchRaw(&x, &y);
  bool touch = tft.getTouch(&x, &y);
  if( touch ) {
    Serial.printf("x: %i     ", x);
    Serial.printf("y: %i     ", y);
    Serial.printf("z: %i \n", tft.getTouchRawZ());
  }

  // handle stuff efvery 1 second
  if( currentTime >= nextTime ) {
    if( color ) {
      tft.fillScreen( TFT_BLUE );
    }
    else {
      tft.fillScreen( TFT_GREEN );
    }

    color = !color;
    nextTime += 1000;

    Serial.print( "." );
  }
}
