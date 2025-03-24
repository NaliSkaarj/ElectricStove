#ifndef _OTA_H
#define _OTA_H

#define PORT                23     // port for telnet connections
#define OTA_HOST_NAME       "ElectricStove"
#define OTA_STACK_SIZE      2048// number of words, at 1965 OTA OK, at 1964 OTA failing sometimes
#define OTA_TASK_PRIORITY   1

typedef void (* otaActiveCb)( bool );

/**
 * Need to be called from main Setup/Init function to run the service
 */
void OTA_Init();

/**
 * Set a callback function that will be called when OTA change state
 * func         -   callback function
 */
void OTA_setOtaActiveCallback( otaActiveCb func );

/**
 * Turn on/off OTA service
 * active       -   whether to activate OTA
 */
void OTA_Activate( bool active );

/**
 * Send log to client (connected over telnet)
 * buf/x        - pointer to string or int that will be send to client
 */
void OTA_LogWrite( const char *buf );
void OTA_LogWrite( const int x );

#endif  // _OTA_H