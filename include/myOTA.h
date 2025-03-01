#ifndef _OTA_H
#define _OTA_H

#define PORT 23     // port for telnet connections
#define OTA_HOST_NAME "ElectricStove"

/**
 * Need to be called from main Setup/Init function to run the service
 */
void OTA_Setup();

/**
 * Need to be called in loop() to run the service
 */
void OTA_Handle();

/**
 * Turn on OTA service
 */
void OTA_On();

/**
 * Turn off OTA service
 */
void OTA_Off();

/**
 * Send log to client (connected over telnet)
 * buf/x        - pointer to string or int that will be send to client
 */
void OTA_LogWrite( const char *buf );
void OTA_LogWrite( const int x );

#endif  // _OTA_H