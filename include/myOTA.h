#ifndef MYOTA_H
#define MYOTA_H

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
 * Send log to client (connected over telnet)
 */
void OTA_LogWrite( const char *buf );

#endif  // MYOTA_H