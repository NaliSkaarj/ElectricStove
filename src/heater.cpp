#include <Arduino.h>
#include "heater.h"
#include "PID.h"
#include "max6675.h"
#include "buzzer.h"
#include "myOTA.h"

#define BAD_TEMP_CNT_RISE_ERROR 100

typedef enum heaterStates {
  HEATING_STOP = 0,
  HEATING_PROCESSING,
  HEATING_PAUSE,
  HEATING_STATE_COUNT
} heaterStates;

static heaterStates       heaterState = HEATING_STOP;     // quarded by mutex
static bool               initialized = false;
static uint32_t           heatingTempRequested = 0;
static uint32_t           heatingTimeRequested = 0;       // quarded by mutex
static uint32_t           heatingTimeStart;               // quarded by mutex
static uint32_t           heatingTimeStop;
static uint32_t           heatingTimePauseTotal = 0;
static uint32_t           heatingTimePauseStart;
static volatile float     currentTemperature = 0.0f;
static heaterDoneCb       funcDoneCB = NULL;
static uint32_t           failSemaphoreCounter = 0;       // debug purpose only
static SemaphoreHandle_t  xSemaphore = NULL;
static StaticSemaphore_t  xMutexBuffer;
static TaskHandle_t       taskHandle = NULL;
static StaticTask_t       taskTCB;
static StackType_t        taskStack[ HEATER_STACK_SIZE ];

static void vTaskHeater( void * pvParameters );
static void heaterHandle( void );

static void vTaskHeater( void * pvParameters ) {
  static uint8_t badTemparatureCount = 0;
  static uint32_t buzzId;

  while( 1 ) {
    float currTemp = MAX6675_readCelsius();

    if( MIN_ALLOWED_TEMP <= currTemp
    && MAX_ALLOWED_TEMP >= currTemp
    ) {
      currentTemperature = currTemp;
      if( 0 < buzzId ) {
        BUZZ_Delete( buzzId );      // stop buzzing if no temp error
        buzzId = 0;
      }
    } else {
      badTemparatureCount++;
      if( BAD_TEMP_CNT_RISE_ERROR < badTemparatureCount ) {
        currentTemperature = MAX_ALLOWED_TEMP;    // this force PID to heating less
        badTemparatureCount = 0;
        if( 0 == buzzId ) {
          buzzId = BUZZ_Add( 0, 200, 100, UINT32_MAX );   // buzzing continuously
        }
        Serial.println( "HEATER(task): max6675 temp read fail\n" );
      }
    }
    heaterHandle();

    vTaskDelay( PID_INTERVAL_COMPUTE / portTICK_PERIOD_MS );
  }
}

static void heaterHandle() {
  if( pdTRUE == xSemaphoreTake( xSemaphore, portMAX_DELAY ) ) {
    switch( heaterState ) {
      case HEATING_STOP: {
        // nothing to do
        break;
      }

      case HEATING_PROCESSING: {
        if( millis() >= ( heatingTimeStart + heatingTimeRequested + heatingTimePauseTotal ) ) {  // time is up
          PID_Off();
          heaterState = HEATING_STOP;

          if( NULL != funcDoneCB ) {
            funcDoneCB();
          }

          break;
        }

        PID_updateTemp( (double)currentTemperature );
        PID_Compute();
        break;
      }

      case HEATING_PAUSE: {
        // nothing to do
        break;
      }
    }

    xSemaphoreGive( xSemaphore );
  } else {
    failSemaphoreCounter++;
    Serial.println( "HEATER(Handle): couldn't take semaphore " + (String)failSemaphoreCounter + " times" );
  }
}

void HEATER_Init( SPIClass * spi ) {
  if( true == initialized ) {
    return;
  }

  xSemaphore = xSemaphoreCreateMutexStatic( &xMutexBuffer );
  assert( xSemaphore );

  taskHandle = xTaskCreateStatic( vTaskHeater, "Heater", HEATER_STACK_SIZE, NULL, HEATER_TASK_PRIORITY, taskStack, &taskTCB );
  assert( taskHandle );

  PID_Init();
  MAX6675_Init( spi, HEATER_MAX6675_CS );

  initialized = true;
}

void HEATER_setTemperature( uint16_t temp ) {
  if( false == initialized ) {
    return;
  }

  if( pdTRUE == xSemaphoreTake( xSemaphore, portMAX_DELAY ) ) {
    heatingTempRequested = ( MAX_ALLOWED_TEMP < temp ) ? MAX_ALLOWED_TEMP : temp;

    xSemaphoreGive( xSemaphore );
  } else {
    failSemaphoreCounter++;
    Serial.println( "HEATER(setTemperature): couldn't take semaphore " + (String)failSemaphoreCounter + " times" );
  }
}

void HEATER_setTime( uint32_t time ) {
  if( false == initialized ) {
    return;
  }

  if( pdTRUE == xSemaphoreTake( xSemaphore, portMAX_DELAY ) ) {
    heatingTimeRequested = time;

    xSemaphoreGive( xSemaphore );
  } else {
    failSemaphoreCounter++;
    Serial.println( "HEATER(setTime): couldn't take semaphore " + (String)failSemaphoreCounter + " times" );
  }
}

void HEATER_setTempTime( uint16_t temp, uint32_t time ) {
  if( false == initialized ) {
    return;
  }

  HEATER_setTemperature( temp );
  HEATER_setTime( time );
}

void HEATER_start( void ) {
  if( false == initialized ) {
    return;
  }

  if( pdTRUE == xSemaphoreTake( xSemaphore, portMAX_DELAY ) ) {
    switch( heaterState ) {
      case HEATING_STOP: {
        PID_SetPoint( heatingTempRequested );
        heatingTimeStart = millis();
        heatingTimePauseTotal = 0;
        PID_On();
        heaterState = HEATING_PROCESSING;
        break;
      }

      // case HEATING_PAUSE: use HEATER_pause() for this case
      default: {
        // nothing to do
        break;
      }
    }

    xSemaphoreGive( xSemaphore );
  } else {
    failSemaphoreCounter++;
    Serial.println( "HEATER(start): couldn't take semaphore " + (String)failSemaphoreCounter + " times" );
  }
}

void HEATER_pause( void ) {
  if( false == initialized ) {
    return;
  }

  if( pdTRUE == xSemaphoreTake( xSemaphore, portMAX_DELAY ) ) {
    switch( heaterState ) {
      case HEATING_PROCESSING: {
        PID_Off();
        heatingTimePauseStart = millis();
        heaterState = HEATING_PAUSE;
        break;
      }

      case HEATING_PAUSE: {
        // recontinue processing
        PID_On();
        heatingTimePauseTotal += ( millis() - heatingTimePauseStart );
        Serial.printf( "Pause time total: %d\n", heatingTimePauseTotal );
        heaterState = HEATING_PROCESSING;
        break;
      }

      default: {
        // nothing to do
        break;
      }
    }

    xSemaphoreGive( xSemaphore );
  } else {
    failSemaphoreCounter++;
    Serial.println( "HEATER(pause): couldn't take semaphore " + (String)failSemaphoreCounter + " times" );
  }
}

void HEATER_stop( void ) {
  if( false == initialized ) {
    return;
  }

  if( pdTRUE == xSemaphoreTake( xSemaphore, portMAX_DELAY ) ) {
    switch( heaterState ) {
      case HEATING_PROCESSING:
      case HEATING_PAUSE: {
        PID_Off();
        heaterState = HEATING_STOP;
        break;
      }

      default: {
        // nothing to do
        break;
      }
    }

    xSemaphoreGive( xSemaphore );
  } else {
    failSemaphoreCounter++;
    Serial.println( "HEATER(stop): couldn't take semaphore " + (String)failSemaphoreCounter + " times" );
  }
}

void HEATER_setCallback( heaterDoneCb func ) {
  if( false == initialized ) {
    return;
  }

  if( NULL != func ) {
    funcDoneCB = func;
  }
}

float HEATER_getCurrentTemperature( void ) {
  return currentTemperature;
}

uint32_t HEATER_getTimeRemaining() {
  int32_t result = 0;

  if( false == initialized ) {
    return 0;
  }

  if( pdTRUE == xSemaphoreTake( xSemaphore, portMAX_DELAY ) ) {
    switch( heaterState ) {
      case HEATING_STOP: {
        // nothing to do
        break;
      }

      case HEATING_PROCESSING: {
        result = heatingTimeRequested - ( millis() - ( heatingTimeStart + heatingTimePauseTotal ));
        break;
      }

      case HEATING_PAUSE: {
        result = heatingTimeRequested - ( heatingTimePauseStart - ( heatingTimeStart + heatingTimePauseTotal ) );
        break;
      }
    }
    xSemaphoreGive( xSemaphore );
  } else {
    failSemaphoreCounter++;
    Serial.println( "HEATER(getTimeRemaining): couldn't take semaphore " + (String)failSemaphoreCounter + " times" );
  }

  return ( 0 > result ) ? 0 : (uint32_t)result;
}

uint8_t HEATER_getCurrentPower() {
  return PID_getOutputPercentage();
}

bool HEATER_isHeating() {
  return PID_isHeaterActive();
}
