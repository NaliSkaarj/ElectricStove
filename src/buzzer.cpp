#include "buzzer.h"
#include <Arduino.h>
#include "myOTA.h"

typedef struct buzzer
{
  unsigned int    hash;           // primitive hash to distinguish items
  unsigned long   start;
  unsigned long   period;
  unsigned long   repeatDelay;
  unsigned int    repeatCount;
  bool            active;
} Buzzer_t;

static Buzzer_t           buzzerList[ BUZZ_BUZZERS_MAX ];   // guarded by mutex
static unsigned int       idx;
static unsigned long      globalTime;
static bool               initialized = false;
static bool               muted = false;
static SemaphoreHandle_t  xSemaphore = NULL;
static StaticSemaphore_t  xMutexBuffer;
static uint32_t           failSemaphoreCounter = 0;   // debug purpose only
static bool               buzzHandleForce = false;
static TaskHandle_t       taskHandle = NULL;
static StaticTask_t       taskTCB;
static StackType_t        taskStack[ BUZZER_STACK_SIZE ];

static unsigned int getNextHash();
static int getFreeSlotIndex();
static void vTaskBuzzer( void * pvParameters );
static void buzzerHandle( unsigned long currentTime );

static unsigned int getNextHash() {
  unsigned int tmpHash = 0;

  if( xSemaphore != NULL ) {
    if( pdTRUE == xSemaphoreTake( xSemaphore, (TickType_t)( 100/portTICK_PERIOD_MS ) ) ) {
      for( int x=0; x<BUZZ_BUZZERS_MAX; x++ ) {
        if( buzzerList[ x ].hash > tmpHash ) {
          tmpHash = buzzerList[ x ].hash;
        }
      }

      xSemaphoreGive( xSemaphore );
    } else {
      failSemaphoreCounter++;
      Serial.println( "BUZZ_hash: couldn't take semaphore " + (String)failSemaphoreCounter + " times" );
      OTA_LogWrite( "BUZZ_hash: couldn't take semaphore: " );
      OTA_LogWrite( failSemaphoreCounter );
      OTA_LogWrite( "\n" );
    }
  } else {
    Serial.println( "BUZZ_hash: semaphore is NULL" );
  }

  return ++tmpHash;
}

static int getFreeSlotIndex() {
  int freeSlotIdx = -1;

  if( xSemaphore != NULL ) {
    if( pdTRUE == xSemaphoreTake( xSemaphore, (TickType_t)( 100/portTICK_PERIOD_MS ) ) ) {
      for( int x=0; x<BUZZ_BUZZERS_MAX; x++ ) {
        if( (0 == buzzerList[ x ].hash) || (false == buzzerList[ x ].active) ) {
          freeSlotIdx = x;
          break;
        }
      }

      xSemaphoreGive( xSemaphore );
    } else {
      failSemaphoreCounter++;
      Serial.println( "BUZZ_slot: couldn't take semaphore " + (String)failSemaphoreCounter + " times" );
      OTA_LogWrite( "BUZZ_slot: couldn't take semaphore: " );
      OTA_LogWrite( failSemaphoreCounter );
      OTA_LogWrite( "\n" );
    }
  } else {
    Serial.println( "BUZZ_slot: semaphore is NULL" );
  }

  return freeSlotIdx;
}

static void vTaskBuzzer( void * pvParameters ) {
  while( 1 ) {
    buzzerHandle( millis() );
    vTaskDelay( 1 / portTICK_PERIOD_MS );
  }
}

static void buzzerHandle( unsigned long currentTime ) {
  uint8_t activateBuzzing = LOW;

  if( false == initialized ) {
    return;
  }

  if( globalTime == currentTime
    && !buzzHandleForce
  ) {
    // no change in time, nothing to do
    return;
  }

  globalTime = currentTime;
  buzzHandleForce = false;

  if( xSemaphore != NULL ) {
    if( pdTRUE == xSemaphoreTake( xSemaphore, (TickType_t)( 100/portTICK_PERIOD_MS ) ) ) {
      for( int x=0; x<BUZZ_BUZZERS_MAX; x++ ) {
        // check against 'buzzing' activation
        if( ( globalTime >= buzzerList[ x ].start )
          && ( globalTime < ( buzzerList[ x ].start + buzzerList[ x ].period ) )
          && ( true == buzzerList[ x ].active )
        ) {
          activateBuzzing = HIGH;
        }

        // check against 'buzzing' deactivation
        if( globalTime >= ( buzzerList[ x ].start + buzzerList[ x ].period ) ) {
          if( 0 < buzzerList[ x ].repeatCount ) {
            buzzerList[ x ].start = buzzerList[ x ].start + buzzerList[ x ].period + buzzerList[ x ].repeatDelay;
            buzzerList[ x ].repeatCount--;
          } else {
            buzzerList[ x ].active = false;
          }
        }
      }
      xSemaphoreGive( xSemaphore );
    } else {
      failSemaphoreCounter++;
      Serial.println( "BUZZ_Handle: couldn't take semaphore " + (String)failSemaphoreCounter + " times" );
      OTA_LogWrite( "BUZZ_Handle: couldn't take semaphore: " );
      OTA_LogWrite( failSemaphoreCounter );
      OTA_LogWrite( "\n" );
    }
  } else {
    Serial.println( "BUZZ_Handle: semaphore is NULL" );
    return;
  }

  if( muted ) {
    digitalWrite( BUZZ_OUTPUT_PIN, LOW );
  } else {
    digitalWrite( BUZZ_OUTPUT_PIN, activateBuzzing );
  }
}

void BUZZ_Init( void ) {
  if( initialized ) {
    return;
  }

  taskHandle = xTaskCreateStatic( vTaskBuzzer, "Buzzer", BUZZER_STACK_SIZE, NULL, BUZZER_TASK_PRIORITY, taskStack, &taskTCB );

  if( NULL == taskHandle ) {
    Serial.println( "BUZZER: Task couldn't be created" );
    return;
  }

  globalTime = millis();

  pinMode( BUZZ_OUTPUT_PIN, OUTPUT );
  digitalWrite( BUZZ_OUTPUT_PIN, LOW );

  for( int x=0; x<BUZZ_BUZZERS_MAX; x++ ) {
    buzzerList[ x ].hash = 0;
    buzzerList[ x ].start = 0;
    buzzerList[ x ].period = 0;
    buzzerList[ x ].repeatDelay = 0;
    buzzerList[ x ].repeatCount = 0;
    buzzerList[ x ].active = false;
  }

  xSemaphore = xSemaphoreCreateMutexStatic( &xMutexBuffer );

  initialized = true;
}

unsigned int BUZZ_Add( unsigned long startDelay, unsigned long period, unsigned long repeatDelay, int repeat ) {
  if( (false == initialized) || (0 == period) ) {
    return 0;
  }

  int freeSlotIdx = getFreeSlotIndex();
  unsigned int highestHash = getNextHash();

  if( 0 > freeSlotIdx ) {
    // Serial.println( "BUZZ_Add: no free slot available." );
    return 0;
  }

  if( UINT_MAX == highestHash ) {
    Serial.println( "BUZZ_Add: max hash number riched. No implementation for such situation." );
    // some garbage collector could be triggered here
    return 0;
  }

  if( xSemaphore != NULL ) {
    if( pdTRUE == xSemaphoreTake( xSemaphore, (TickType_t)( 100/portTICK_PERIOD_MS ) ) ) {
      buzzerList[ freeSlotIdx ].hash = highestHash;
      buzzerList[ freeSlotIdx ].start = globalTime + startDelay;
      buzzerList[ freeSlotIdx ].period = period;
      buzzerList[ freeSlotIdx ].repeatDelay = repeatDelay;
      buzzerList[ freeSlotIdx ].repeatCount = repeat ? repeat-1 : 0;   // repeat only repeat-1 times (one is by default thus -1)
      buzzerList[ freeSlotIdx ].active = true;

      xSemaphoreGive( xSemaphore );
    } else {
      failSemaphoreCounter++;
      Serial.println( "BUZZ_Add: couldn't take semaphore " + (String)failSemaphoreCounter + " times" );
      OTA_LogWrite( "BUZZ_Add: couldn't take semaphore: " );
      OTA_LogWrite( failSemaphoreCounter );
      OTA_LogWrite( "\n" );
    }
  } else {
    Serial.println( "BUZZ_Add: semaphore is NULL" );
  }

  buzzHandleForce = true;
  buzzerHandle( globalTime );  // force activating buzzer (if needed)

  return 0;
}

unsigned int BUZZ_Add( unsigned long period, unsigned long repeatDelay, int repeat ) {
  return BUZZ_Add( 0, period, repeatDelay, repeat );
}

unsigned int BUZZ_Add( unsigned long period ) {
  return BUZZ_Add( 0, period, 0, 0 );
}

bool BUZZ_Delete( int handle ) {
  bool retValue = false;

  if( false == initialized ) {
    return false;
  }

  if( xSemaphore != NULL ) {
    if( pdTRUE == xSemaphoreTake( xSemaphore, (TickType_t)( 100/portTICK_PERIOD_MS ) ) ) {
      for( int x=0; x<BUZZ_BUZZERS_MAX; x++ ) {
        if( handle == buzzerList[ x ].hash ) {
          buzzerList[ x ].hash = 0;
          buzzerList[ x ].active = false;
          retValue = true;
          buzzHandleForce = true;
          buzzerHandle( globalTime );  // recalculate list of 'buzzings'
          break;
        }
      }

      xSemaphoreGive( xSemaphore );
    } else {
      failSemaphoreCounter++;
      Serial.println( "BUZZ_Delete: couldn't take semaphore " + (String)failSemaphoreCounter + " times" );
      OTA_LogWrite( "BUZZ_Delete: couldn't take semaphore: " );
      OTA_LogWrite( failSemaphoreCounter );
      OTA_LogWrite( "\n" );
    }
  } else {
    Serial.println( "BUZZ_Delete: semaphore is NULL" );
  }

  return retValue;
}

void BUZZ_Activate( bool active ) {
  muted = !active;
}
