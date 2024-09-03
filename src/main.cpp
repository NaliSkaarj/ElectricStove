#include <Arduino.h>
#include "TFT_eSPI.h"

uint16_t x, y;
bool color = true;
unsigned long currentTime, nextTime;

TFT_eSPI tft = TFT_eSPI();

// Code to run a screen calibration, not needed when calibration values are set in setup()
void touch_calibrate()
{
  uint16_t calData[5];

  // Calibrate
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(20, 0);
  tft.setTextFont(2);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  tft.println("Touch corners as indicated");

  tft.setTextFont(1);
  tft.println();

  tft.calibrateTouch(calData, TFT_MAGENTA, TFT_BLACK, 15);

  Serial.println(); Serial.println();
  Serial.println("// Use this calibration code in setup():");
  Serial.print("  uint16_t calData[5] = ");
  Serial.print("{ ");

  for (uint8_t i = 0; i < 5; i++)
  {
    Serial.print(calData[i]);
    if (i < 4) Serial.print(", ");
  }

  Serial.println(" };");
  Serial.print("  tft.setTouch(calData);");
  Serial.println(); Serial.println();

  tft.fillScreen(TFT_BLACK);
  
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.println("Calibration complete!");
  tft.println("Calibration code sent to Serial port.");

  delay(10000);
}

void setup() {
  Serial.begin( 115200 );
  tft.init();
  tft.setRotation( 3 );

  touch_calibrate();
  // uint16_t calData[5] = { 265, 3677, 261, 3552, 1 };
  // tft.setTouch(calData);

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
