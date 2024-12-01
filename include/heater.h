#ifndef _HEATER_H
#define _HEATER_H

#define HEATER_STACK_SIZE       2048
#define HEATER_TASK_PRIORITY    3
#define HEATER_MAX6675_CLK      18
#define HEATER_MAX6675_MISO     19
#define HEATER_MAX6675_CS       9
#define MINUTES_TO_MS(m)        ( (m) * 60 * 1000)

typedef void (* heaterDoneCb)( void );

/**
 * Need to be called from main Setup/Init function to run the service
 */
void HEATER_Init( void );

/**
 * Set target temperature
 * temp         -   temperature to be reached
 */
void HEATER_setTemperature( uint16_t temp );

/**
 * Set time period
 * time         -   the time for which the temperature will be maintained
 */
void HEATER_setTime( uint32_t time );

/**
 * Set target temperature and time period
 * temp         -   temperature to be reached
 * time         -   the time for which the temperature will be maintained
 */
void HEATER_setTempTime( uint16_t temp, uint32_t time );

/**
 * Start heating process
 */
void HEATER_start( void );

/**
 * Pause heating process
 */
void HEATER_pause( void );

/**
 * Stop heating process
 */
void HEATER_stop( void );

/**
 * Set a callback function that will be called when the heating process is completed
 * heaterDoneCb -   callback function
 */
void HEATER_setCallback( heaterDoneCb );

/**
 * Get furnace temperature
 * return(float)    - current temperature (can be NaN)
 */
float HEATER_getCurrentTemperature( void );

/**
 * Get heating remaining time
 * return(uint32_t) - how much time remaining in current phase of heating (one heating process can have few heating phases)
 */
uint32_t HEATER_getTimeRemaining();

#endif  // _HEATER_H
