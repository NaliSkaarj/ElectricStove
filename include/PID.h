#ifndef _PID_H
#define _PID_H

#include <Arduino.h>

#define PID_PIN_RELAY           25
#define PID_WINDOW_SIZE         5000
#define PID_DEADTIME_SIZE       0
#define PID_INTERVAL_COMPUTE    100     // the period (in ms) at which the calculation is performed

void PID_Init();
void PID_Compute();
void PID_SetPoint( uint16_t targetPoint );
void PID_On();
void PID_Off();
void PID_updateTemp( double temp );
uint8_t PID_getOutputPercentage();
bool PID_isHeaterActive();

#endif  // _PID_H
