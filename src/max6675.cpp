#include "Arduino.h"
#include "max6675.h"
#include "SPI.h"

static int8_t cs;         // chip select pin
static bool   initialized = false;
static volatile float     currentTemperature;
static TaskHandle_t       taskHandle = NULL;
static StaticTask_t       taskTCB;
static StackType_t        taskStack[ MAX6675_STACK_SIZE ];

SPIClass * sharedSpi;

static void vTaskHeater( void * pvParameters );

static void vTaskHeater( void * pvParameters ) {
  while( 1 ) {
    if( initialized ) {
      uint16_t v;

      sharedSpi->beginTransaction( SPISettings( 4000000, MSBFIRST, SPI_MODE0 ) );  // max SPI speed used succesfully:25MHz
      digitalWrite( cs, LOW );
      v = sharedSpi->transfer16( 0x00 );
      digitalWrite( cs, HIGH );
      sharedSpi->endTransaction();

      if ( v & 0x4 ) {
        // no thermocouple attached!
        currentTemperature = NAN;
      } else {
        v >>= 3;
        currentTemperature = v * 0.25;
      }
    } else {
      currentTemperature = NAN;
    }

    vTaskDelay( TEMP_READ_INTERVAL / portTICK_PERIOD_MS );
  }
}

void MAX6675_Init( SPIClass * spi, int8_t _CS ) {
  if( true == initialized ) {
    return;
  }

  if( NULL == spi ) {
    return;
  }

  cs = _CS;
  sharedSpi = spi;

  pinMode( cs, OUTPUT );
  digitalWrite( cs, HIGH );

  sharedSpi->begin();

  taskHandle = xTaskCreateStatic( vTaskHeater, "Heater", MAX6675_STACK_SIZE, NULL, MAX6675_TASK_PRIORITY, taskStack, &taskTCB );

  initialized = true;
}

float MAX6675_readCelsius( void ) {
  return currentTemperature;
}
