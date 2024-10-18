#include "buzzer.h"
#include <Arduino.h>

/*
FIXME: module not proof against multi-threading (RTOS)
*/

typedef struct buzzer
{
  unsigned int    hash;           // primitive hash to distinguish items
  unsigned long   start;
  unsigned long   period;
  unsigned long   repeatDelay;
  unsigned int    repeatCount;
  bool            active;
} Buzzer_t;

static Buzzer_t         buzzerList[ BUZZ_BUZZERS_MAX ];
static unsigned int     idx;
static unsigned long    globalTime;
static bool             initialized = false;

static unsigned int getNextHash() {
  unsigned int tmpHash = 0;

  for( int x=0; x<BUZZ_BUZZERS_MAX; x++) {
    if( buzzerList[x].hash > tmpHash ) {
      tmpHash = buzzerList[x].hash;
    }
  }
  return ++tmpHash;
}

static int getFreeSlotIndex() {
  int freeSlotIdx = -1;

  for( int x=0; x<BUZZ_BUZZERS_MAX; x++) {
    if( (0 == buzzerList[ x ].hash) || (false == buzzerList[ x ].active) ) {
      freeSlotIdx = x;
      break;
    }
  }
  return freeSlotIdx;
}

void BUZZ_Init( unsigned long currentTime ) {
  if( initialized ) {
    return;
  }

  globalTime = currentTime;

  pinMode( BUZZ_OUTPUT_PIN, OUTPUT );
  digitalWrite( BUZZ_OUTPUT_PIN, LOW );

  for( int x=0; x<BUZZ_BUZZERS_MAX; x++) {
    buzzerList[ x ].hash = 0;
    buzzerList[ x ].start = 0;
    buzzerList[ x ].period = 0;
    buzzerList[ x ].repeatDelay = 0;
    buzzerList[ x ].repeatCount = 0;
    buzzerList[ x ].active = false;
  }

  initialized = true;
}

void BUZZ_Handle( unsigned long currentTime ) {
  bool activateBuzzing = false;

  if( false == initialized ) {
    return;
  }

  globalTime = currentTime;

  for( int x=0; x<BUZZ_BUZZERS_MAX; x++) {
    // check against 'buzzing' activation
    if( ( globalTime >= buzzerList[ x ].start )
      && ( globalTime < (buzzerList[ x ].start + buzzerList[ x ].period) )
    ) {
      activateBuzzing = true;
    }

    // check against 'buzzing' deactivation
    if( globalTime >= (buzzerList[ x ].start + buzzerList[ x ].period) ) {
      //deactivateBuzzing = true;
      if( 0 < buzzerList[ x ].repeatCount ) {
        buzzerList[ x ].start = buzzerList[ x ].start + buzzerList[ x ].period + buzzerList[ x ].repeatDelay;
        buzzerList[ x ].repeatCount--;
      } else {
        buzzerList[ x ].active = false;
      }
    }
  }

  if( activateBuzzing ) {
    digitalWrite( BUZZ_OUTPUT_PIN, HIGH );
  } else {
    digitalWrite( BUZZ_OUTPUT_PIN, LOW );
  }
}

unsigned int BUZZ_Add( unsigned long startDelay, unsigned long period, unsigned long repeatDelay, int repeat ) {
  if( (false == initialized) || (0 == period) ) {
    return 0;
  }

  int freeSlotIdx = getFreeSlotIndex();
  unsigned int highestHash = getNextHash();

  if( 0 > freeSlotIdx ) {
    // Serial.println( "BUZZER: no free slot available." );
    return 0;
  }

  if( UINT_MAX == highestHash ) {
    Serial.println( "BUZZER: max hash number riched. No implementation for such situation." );
    // some garbage collector could be triggered here
    return 0;
  }

  buzzerList[ freeSlotIdx ].hash = highestHash;
  buzzerList[ freeSlotIdx ].start = globalTime + startDelay;
  buzzerList[ freeSlotIdx ].period = period;
  buzzerList[ freeSlotIdx ].repeatDelay = repeatDelay;
  buzzerList[ freeSlotIdx ].repeatCount = repeat;
  buzzerList[ freeSlotIdx ].active = true;

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

  for( int x=0; x<BUZZ_BUZZERS_MAX; x++) {
    if( handle == buzzerList[ x ].hash ) {
      buzzerList[ x ].hash = 0;
      buzzerList[ x ].active = false;
      retValue = true;
      BUZZ_Handle( globalTime );  // recalculate list of 'buzzings'
      break;
    }
  }

  return retValue;
}
