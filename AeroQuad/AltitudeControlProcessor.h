/*
  AeroQuad v3.0.1 - February 2012
  www.AeroQuad.com
  Copyright (c) 2012 Ted Carancho.  All rights reserved.
  An Open Source Arduino based multicopter.
 
  This program is free software: you can redistribute it and/or modify 
  it under the terms of the GNU General Public License as published by 
  the Free Software Foundation, either version 3 of the License, or 
  (at your option) any later version. 
 
  This program is distributed in the hope that it will be useful, 
  but WITHOUT ANY WARRANTY; without even the implied warranty of 
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
  GNU General Public License for more details. 
 
  You should have received a copy of the GNU General Public License 
  along with this program. If not, see <http://www.gnu.org/licenses/>. 
*/

// FlightControl.pde is responsible for combining sensor measurements and
// transmitter commands into motor commands for the defined flight configuration (X, +, etc.)
//////////////////////////////////////////////////////////////////////////////
/////////////////////////// calculateFlightError /////////////////////////////
//////////////////////////////////////////////////////////////////////////////

#ifndef _AQ_ALTITUDE_CONTROL_PROCESSOR_H_
#define _AQ_ALTITUDE_CONTROL_PROCESSOR_H_


#if defined AltitudeHoldBaro || defined AltitudeHoldRangeFinder

#define INVALID_THROTTLE_CORRECTION -1000
#define ALTITUDE_BUMP_SPEED 0.01

#ifndef MAX
  #define MAX(A,B) ((A)>(B) ? (A) : (B))
#endif


/**
 * processAltitudeHold
 * 
 * This function is responsible to process the throttle correction 
 * to keep the current altitude if selected by the user 
 */
void processAltitudeHold()
{
  // ****************************** Altitude Adjust *************************
  // Thanks to Honk for his work with altitude hold
  // http://aeroquad.com/showthread.php?792-Problems-with-BMP085-I2C-barometer
  // Thanks to Sherbakov for his work in Z Axis dampening
  // http://aeroquad.com/showthread.php?359-Stable-flight-logic...&p=10325&viewfull=1#post10325

  if (ALT_BARO == altitudeHoldMode || ALT_SONAR == altitudeHoldMode) {

    float PIDComponents[3];
    // simulatePID(altitudeToHoldTarget, altitude, &PID[ALTITUDE_HOLD_SPEED_PID_IDX], PIDComponents);
    // logger.log(currentTime, DataLogger::altitudeSpeedPID_Pr, PIDComponents[0]);
    // logger.log(currentTime, DataLogger::altitudeSpeedPID_Ir, PIDComponents[1]);

    static float previousAltitudeToHoldTarget = 0.0;
    const float dT = 1.0/50.0;
    float altitudeToHoldTargetROC = (altitudeToHoldTarget - previousAltitudeToHoldTarget) / dT;
    previousAltitudeToHoldTarget = altitudeToHoldTarget;
    altitudeToHoldTargetROC = constrain(altitudeToHoldTargetROC, -1.0, 1.0);

    float vDeadBand = 0.125;
    float altitudeError = fabsf(altitudeToHoldTarget - altitude);
    if (altitudeError > vDeadBand) {
      const float a = 5.0; // Deceleration rate as we approach target altitude, m/s^2.
      targetVerticalSpeed = a * sqrtf(2.0 * (altitudeError-vDeadBand) / a);
      if (altitudeToHoldTarget < altitude)
        targetVerticalSpeed = -targetVerticalSpeed;
    } else {
      targetVerticalSpeed = updatePIDAlternate(altitudeToHoldTarget, altitude, &PID[ALTITUDE_HOLD_SPEED_PID_IDX], NULL);
    }
    targetVerticalSpeed += altitudeToHoldTargetROC;

    const float vMax = 5.0; // Maximum target speed, m/s.
    targetVerticalSpeed = constrain(targetVerticalSpeed, -vMax, vMax);

    logger.log(currentTime, DataLogger::targetVerticalSpeed, targetVerticalSpeed);

    // simulatePID(targetVerticalSpeed, speed, &PID[ALTITUDE_HOLD_THROTTLE_PID_IDX], PIDComponents);
    // simulatePID(targetVerticalSpeed, verticalSpeed, &PID[ALTITUDE_HOLD_THROTTLE_PID_IDX], PIDComponents);

    // altitudeHoldThrottleCorrectionRaw = updatePID(targetVerticalSpeed, speed, &PID[ALTITUDE_HOLD_THROTTLE_PID_IDX]);
    altitudeHoldThrottleCorrection = updatePIDAlternate(targetVerticalSpeed, verticalSpeed, &PID[ALTITUDE_HOLD_THROTTLE_PID_IDX], PIDComponents);
    altitudeHoldThrottleCorrection = constrain(altitudeHoldThrottleCorrection, minThrottleAdjust, maxThrottleAdjust);
    logger.log(currentTime, DataLogger::altitudeThrottlePID_Pr, PIDComponents[0]);
    logger.log(currentTime, DataLogger::altitudeThrottlePID_Ir, PIDComponents[1]);
    logger.log(currentTime, DataLogger::altitudeThrottlePID_Dr, PIDComponents[2]);
    logger.log(currentTime, DataLogger::altitudeHoldThrottleCorrection, altitudeHoldThrottleCorrection);

    if (abs(altitudeHoldThrottle - receiverCommand[THROTTLE]) > altitudeHoldPanicStickMovement) {
      altitudeHoldMode = ALT_PANIC; // too rapid of stick movement so PANIC out of ALTHOLD
      altitudeHoldThrottleCorrection = 0;
      logger.log(currentTime, DataLogger::altitudeHoldThrottleCorrection, altitudeHoldThrottleCorrection);
    }
    else {

      float altitudeBump = 0.0;
      if (receiverCommand[THROTTLE] > (altitudeHoldThrottle + altitudeHoldBump))
        altitudeBump = ALTITUDE_BUMP_SPEED;
      else if (receiverCommand[THROTTLE] < (altitudeHoldThrottle - altitudeHoldBump))
        altitudeBump = -ALTITUDE_BUMP_SPEED;

      #if defined AltitudeHoldBaro
        if (ALT_BARO == altitudeHoldMode) {
          altitudeToHoldTarget += altitudeBump;
          logger.log(currentTime, DataLogger::altitudeToHoldTarget, altitudeToHoldTarget);
        }
      #endif
      #if defined AltitudeHoldRangeFinder
        if (ALT_SONAR == altitudeHoldMode) {
          float newalt = altitudeToHoldTarget + altitudeBump;
          if (isOnRangerRange(newalt)) {
            altitudeToHoldTarget = newalt;
            logger.log(currentTime, DataLogger::altitudeToHoldTarget, altitudeToHoldTarget);
          }
        }
      #endif
    }
    throttle = altitudeHoldThrottle + altitudeHoldThrottleCorrection;

    //  Increase throttle to compensate for pitch or roll up to 45 degrees.
    throttle = throttle * (1.0 / MAX(0.707107, down[2]));

    logger.log(currentTime, DataLogger::throttle, throttle);
  }
  else {
    throttle = receiverCommand[THROTTLE];
    logger.log(currentTime, DataLogger::receiverCommandThrottle, receiverCommand[THROTTLE]);
  }
}

#endif

#endif // _AQ_ALTITUDE_CONTROL_PROCESSOR_H_
