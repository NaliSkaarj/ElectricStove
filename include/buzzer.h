#ifndef BUZZER_H
#define BUZZER_H

#define BUZZ_OUTPUT_PIN     33
#define BUZZ_BUZZERS_MAX    10

/**
 * Need to be called from main Setup/Init function to run the service
 * currentTime  -   current time [milliseconds]
 */
void BUZZ_Init( unsigned long currentTime );

/**
 * Need to be called in superloop() to run the service
 * currentTime  -   current time [milliseconds]
 */
void BUZZ_Handle( unsigned long currentTime );

/**
 * Add 'buzzing' to the list, it will be triggered according to provided parameters
 * startDelay   -   'buzzing' will be triggered after this time [milliseconds]
 * period       -   'buzzing' active time [milliseconds]
 * repeatDelay  -   repeat 'buzzing' after this time [milliseconds]
 * repeat       -   repeat count
 * 
 * return       -   (unsigned int) 'buzzing' number (handle)
 *                  0: when adding failed (ie. 'buzzing' list full, module not initialized, 'buzzing' parameter's incorrect)
 */
unsigned int BUZZ_Add( unsigned long startDelay, unsigned long period, unsigned long repeatDelay, int repeat );
unsigned int BUZZ_Add( unsigned long period, unsigned long repeatDelay, int repeat );
unsigned int BUZZ_Add( unsigned long period );

/**
 * Delete 'buzzing' from the list
 * handle       -   handle to 'buzzing' item on the list
 * 
 * return       -   (bool) True: delete success
 *                  False: delete failed (ie. no 'buzzing' found (the item can be already deleted when become inactive))
 */
bool BUZZ_Delete( int handle );

#endif  // BUZZER_H
