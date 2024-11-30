#include <Arduino.h>
#include <PID.h>
#include <PID_v1.h>

#define TOTAL_WINDOW_SIZE   ( PID_WINDOW_SIZE + PID_DEADTIME_SIZE )

static unsigned long windowStartTime, currentTime;
static double setPoint, input, output;
static bool isOn;
static double Kp=70, Ki=0.1, Kd=1000;   //CDHW methode from [https://newton.ex.ac.uk/teaching/CDHW/Feedback/Setup-PID.html]
// static double Kp=2, Ki=5, Kd=1;

PID myPID( &input, &output, &setPoint, Kp, Ki, Kd, DIRECT );

void PID_Init() {
  pinMode( PID_PIN_RELAY, OUTPUT );
  digitalWrite( PID_PIN_RELAY, LOW );
  isOn = false;
  windowStartTime = millis();
  setPoint = 20;
  output = 0.0;
  myPID.SetOutputLimits( 0, PID_WINDOW_SIZE );
  myPID.SetSampleTime( PID_INTERVAL_COMPUTE );
  myPID.SetMode( AUTOMATIC );
}

void PID_Compute() {
  if( !isOn ) {
    digitalWrite( PID_PIN_RELAY, LOW );
    return;
  }

  // input = analogRead( PID_PIN_INPUT );
  myPID.Compute();
  currentTime = millis();

  if( currentTime - windowStartTime > TOTAL_WINDOW_SIZE )
  { // it's time to shift the Relay Window
    windowStartTime += TOTAL_WINDOW_SIZE;
  }

  if( output < currentTime - windowStartTime ) {
    digitalWrite( PID_PIN_RELAY, HIGH );
  }
  else {
    digitalWrite( PID_PIN_RELAY, LOW );
  }
}

void PID_SetPoint( uint16_t targetPoint ) {
  setPoint = (double)targetPoint;
}

void PID_On() {
  isOn = true;
}

void PID_Off() {
  digitalWrite( PID_PIN_RELAY, LOW );
  isOn = false;
}

void PID_updateTemp( double temp ) {
  input = temp;
}
