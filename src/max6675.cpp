#include "Arduino.h"
#include "max6675.h"
#include "SPI.h"

static int8_t cs;
static bool   initialized = false;
static volatile float     currentTemperature;
static TaskHandle_t       taskHandle = NULL;
static StaticTask_t       taskTCB;
static StackType_t        taskStack[ MAX6675_STACK_SIZE ];

SPIClass mySpi = SPIClass( VSPI );

static void vTaskHeater( void * pvParameters );

static void vTaskHeater( void * pvParameters ) {
  while( 1 ) {
    if( initialized ) {
      uint16_t v;

      mySpi.beginTransaction( SPISettings( 4000000, MSBFIRST, 0 ) );  // max SPI speed used succesfully:25MHz
      digitalWrite( cs, LOW );
      v = mySpi.transfer16( 0x00 );
      digitalWrite( cs, HIGH );
      mySpi.endTransaction();

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

void MAX6675_Init( int8_t _SCLK, int8_t _CS, int8_t _MISO ) {
  if( true == initialized ) {
    return;
  }

  if( 0 >= _SCLK || 0 >= _CS || 0 >= _MISO ) {
    return;
  }

  cs = _CS;

  pinMode( cs, OUTPUT );
  digitalWrite( cs, HIGH );

  mySpi.begin( _SCLK, _MISO, MOSI, cs );

  taskHandle = xTaskCreateStatic( vTaskHeater, "Heater", MAX6675_STACK_SIZE, NULL, MAX6675_TASK_PRIORITY, taskStack, &taskTCB );

  initialized = true;
}

float MAX6675_readCelsius( void ) {
  return currentTemperature;
}
