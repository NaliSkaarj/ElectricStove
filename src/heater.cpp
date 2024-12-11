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
  static uint8_t badTemparature = 0;

  while( 1 ) {
    float currTemp = MAX6675_readCelsius();

    if( MIN_ALLOWED_TEMP <= currTemp
    && MAX_ALLOWED_TEMP >= currTemp
    ) {
      currentTemperature = currTemp;
      heaterHandle();
    } else {
      badTemparature++;
      currentTemperature = 0.0f;
      if( BAD_TEMP_CNT_RISE_ERROR < badTemparature ) {
        badTemparature = 0;
        BUZZ_Add( 0, 200, 100, 10 );
        OTA_LogWrite( "HEATER(task): max6675 temp read fail\n" );
      }
    }

    vTaskDelay( PID_INTERVAL_COMPUTE / portTICK_PERIOD_MS );
  }
}

static void heaterHandle() {
  if( xSemaphore != NULL ) {
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
          OTA_LogWrite( "HEATER(Handle): case not handled yet #3\n" );
          break;
        }
      }

      xSemaphoreGive( xSemaphore );
    } else {
      failSemaphoreCounter++;
      OTA_LogWrite( "HEATER(Handle): couldn't take semaphore: " );
      OTA_LogWrite( failSemaphoreCounter );
      OTA_LogWrite( "\n" );
    }
  } else {
    OTA_LogWrite( "HEATER(Handle): semaphore is NULL\n" );
  }
}

void HEATER_Init( void ) {
  if( true == initialized ) {
    return;
  }

  taskHandle = xTaskCreateStatic( vTaskHeater, "Heater", HEATER_STACK_SIZE, NULL, HEATER_TASK_PRIORITY, taskStack, &taskTCB );

  if( NULL == taskHandle ) {
    OTA_LogWrite( "HEATER: Task couldn't be created\n" );
    initialized = false;
    return;
  }

  PID_Init();
  MAX6675_Init( HEATER_MAX6675_CLK, HEATER_MAX6675_CS, HEATER_MAX6675_MISO );

  xSemaphore = xSemaphoreCreateMutexStatic( &xMutexBuffer );

  initialized = true;
}

void HEATER_setTemperature( uint16_t temp ) {
  if( false == initialized ) {
    return;
  }

  if( xSemaphore != NULL ) {
    if( pdTRUE == xSemaphoreTake( xSemaphore, portMAX_DELAY ) ) {
      heatingTempRequested = ( MAX_ALLOWED_TEMP < temp ) ? MAX_ALLOWED_TEMP : temp;

      xSemaphoreGive( xSemaphore );
    } else {
      failSemaphoreCounter++;
      OTA_LogWrite( "HEATER(setTemperature): couldn't take semaphore: " );
      OTA_LogWrite( failSemaphoreCounter );
      OTA_LogWrite( "\n" );
    }
  } else {
    OTA_LogWrite( "HEATER(setTemperature): semaphore is NULL\n" );
  }
}

void HEATER_setTime( uint32_t time ) {
  if( false == initialized ) {
    return;
  }

  if( xSemaphore != NULL ) {
    if( pdTRUE == xSemaphoreTake( xSemaphore, portMAX_DELAY ) ) {
      heatingTimeRequested = time;

      xSemaphoreGive( xSemaphore );
    } else {
      failSemaphoreCounter++;
      OTA_LogWrite( "HEATER(setTime): couldn't take semaphore: " );
      OTA_LogWrite( failSemaphoreCounter );
      OTA_LogWrite( "\n" );
    }
  } else {
    OTA_LogWrite( "HEATER(setTime): semaphore is NULL\n" );
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

  if( xSemaphore != NULL ) {
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
          OTA_LogWrite( "HEATER(start): default case invoked" );
          break;
        }
      }

      xSemaphoreGive( xSemaphore );
    } else {
      failSemaphoreCounter++;
      OTA_LogWrite( "HEATER(start): couldn't take semaphore: " );
      OTA_LogWrite( failSemaphoreCounter );
      OTA_LogWrite( "\n" );
    }
  } else {
    OTA_LogWrite( "HEATER(start): semaphore is NULL\n" );
  }
}

void HEATER_pause( void ) {
  if( false == initialized ) {
    return;
  }

  if( xSemaphore != NULL ) {
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
          Serial.println( heatingTimePauseTotal );
          heaterState = HEATING_PROCESSING;
          break;
        }

        default: {
          OTA_LogWrite( "HEATER(pause): default case invoked" );
          break;
        }
      }

      xSemaphoreGive( xSemaphore );
    } else {
      failSemaphoreCounter++;
      OTA_LogWrite( "HEATER(pause): couldn't take semaphore: " );
      OTA_LogWrite( failSemaphoreCounter );
      OTA_LogWrite( "\n" );
    }
  } else {
    OTA_LogWrite( "HEATER(pause): semaphore is NULL\n" );
  }
}

void HEATER_stop( void ) {
  if( false == initialized ) {
    return;
  }

  if( xSemaphore != NULL ) {
    if( pdTRUE == xSemaphoreTake( xSemaphore, portMAX_DELAY ) ) {
      switch( heaterState ) {
        case HEATING_PROCESSING:
        case HEATING_PAUSE: {
          PID_Off();
          heaterState = HEATING_STOP;
          break;
        }

        default: {
          OTA_LogWrite( "HEATER(stop): default case invoked" );
          break;
        }
      }

      xSemaphoreGive( xSemaphore );
    } else {
      failSemaphoreCounter++;
      OTA_LogWrite( "HEATER(stop): couldn't take semaphore: " );
      OTA_LogWrite( failSemaphoreCounter );
      OTA_LogWrite( "\n" );
    }
  } else {
    OTA_LogWrite( "HEATER(stop): semaphore is NULL\n" );
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
  uint32_t result = 0;

  if( false == initialized ) {
    return result;
  }

  if( xSemaphore != NULL ) {
    if( pdTRUE == xSemaphoreTake( xSemaphore, portMAX_DELAY ) ) {
      switch( heaterState ) {
        case HEATING_STOP: {
          // nothing to do
          result = 0;
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
      OTA_LogWrite( "HEATER(getTimeRemaining): couldn't take semaphore: " );
      OTA_LogWrite( failSemaphoreCounter );
      OTA_LogWrite( "\n" );
    }
  } else {
    OTA_LogWrite( "HEATER(getTimeRemaining): semaphore is NULL\n" );
  }

  return result;
}
