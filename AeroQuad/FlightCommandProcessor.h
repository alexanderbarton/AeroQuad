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

// FlightCommandProcessor is responsible for decoding transmitter stick combinations
// for setting up AeroQuad modes such as motor arming and disarming

#ifndef _AQ_FLIGHT_COMMAND_READER_
#define _AQ_FLIGHT_COMMAND_READER_



#if defined AltitudeHoldBaro || defined AltitudeHoldRangeFinder
  void processAltitudeHoldStateFromReceiverCommand() {

    //  Figure out desired altitude control mode based on receiver commands.
    //  AUX1 chooses the altitude control mode (off, barometer-based, or rangefinder-based (if enabled))
    //  Selecting position-hold (AUX2) or auto-landing (AUX3) also requests barometer-based altitude
    //  control, but only if AUX1 is off.
    //
    AltitudeHoldMode_t switchSetting = ALT_OFF;
    #if defined AltitudeHoldRangeFinder
      if (receiverCommand[AUX1] < 1750)
        switchSetting = ALT_SONAR;
    #endif
    if (receiverCommand[AUX1] < 1250)
      switchSetting = ALT_BARO;
    #if defined (UseGPSNavigator)
      //  Selecting position hold implies altitude hold.
      if (receiverCommand[AUX2] < 1750 && ALT_OFF == switchSetting)
        switchSetting = ALT_BARO;
    #endif
    #if defined (AutoLanding)
      //  Selecting autolanding implies altitude hold.
      if (receiverCommand[AUX3] < 1750 && ALT_OFF == switchSetting)
        switchSetting = ALT_BARO;
    #endif

    //  If desired mode is different from the current mode, change the current mode.
    //
    if (altitudeHoldMode != switchSetting) {
      logger.log(currentTime, DataLogger::altitudeHoldMode, altitudeHoldMode);
      switch (switchSetting) {
        case ALT_OFF:
          //  Turn altitude mode off and/or reset from a panic state.
          altitudeHoldMode = ALT_OFF;
          altitudeHoldThrottleCorrection = 0;
          logger.log(currentTime, DataLogger::altitudeHoldThrottleCorrection, altitudeHoldThrottleCorrection);
          break;
        case ALT_BARO:
          //  Initialize altitude hold using barometer, but only if we're not panicked.
          if (ALT_PANIC != altitudeHoldMode) {
            altitudeHoldMode = ALT_BARO;
            altitudeToHoldTarget = altitude;
            logger.log(currentTime, DataLogger::altitudeToHoldTarget, altitudeToHoldTarget);
            altitudeHoldThrottle = receiverCommand[THROTTLE];
            logger.log(currentTime, DataLogger::altitudeHoldThrottle, altitudeHoldThrottle);
            altitudeHoldThrottleCorrection = 0;

            //  Reach inside the PID to reset them to a clean state.  Yuk.
            PID[ALTITUDE_HOLD_SPEED_PID_IDX].previousPIDTime = currentTime - 0.020;
            PID[ALTITUDE_HOLD_SPEED_PID_IDX].integratedError = 0;
            PID[ALTITUDE_HOLD_SPEED_PID_IDX].lastError = altitudeToHoldTarget;
            PID[ALTITUDE_HOLD_THROTTLE_PID_IDX].previousPIDTime = currentTime - 0.020;
            PID[ALTITUDE_HOLD_THROTTLE_PID_IDX].integratedError = 0;
            PID[ALTITUDE_HOLD_THROTTLE_PID_IDX].lastError = 0.0;
          }
          break;
        case ALT_SONAR:
    #if defined AltitudeHoldRangeFinder
          //  Initialize altitude hold using range finder, but only if we're not panicked.
          if (ALT_PANIC != altitudeHoldMode) {
            altitudeHoldMode = ALT_SONAR;
            altitudeToHoldTarget = 0.0 + rangeFinderRange[ALTITUDE_RANGE_FINDER_INDEX];
            logger.log(currentTime, DataLogger::altitudeToHoldTarget, altitudeToHoldTarget);
            altitudeHoldThrottle = receiverCommand[THROTTLE];
            logger.log(currentTime, DataLogger::altitudeHoldThrottle, altitudeHoldThrottle);
            altitudeHoldThrottleCorrection = 0;

            //  Reach inside the PIDs to reset them to a clean state.  Yuk.
            PID[ALTITUDE_HOLD_SPEED_PID_IDX].previousPIDTime = currentTime - 0.020;
            PID[ALTITUDE_HOLD_SPEED_PID_IDX].integratedError = 0;
            PID[ALTITUDE_HOLD_SPEED_PID_IDX].lastError = altitudeToHoldTarget;
            PID[ALTITUDE_HOLD_THROTTLE_PID_IDX].previousPIDTime = currentTime - 0.020;
            PID[ALTITUDE_HOLD_THROTTLE_PID_IDX].integratedError = 0;
            PID[ALTITUDE_HOLD_THROTTLE_PID_IDX].lastError = 0.0;
          }
    #endif
          break;
        case ALT_PANIC:
          ; //  Not selectable by switches.
      }
      logger.log(currentTime, DataLogger::altitudeHoldMode, altitudeHoldMode);
    }

    #if 0
      //  Increase/decrease altitude target on rising/falling edge of AUX2.
      //  This is useful to test how altitude hold responds to large step functions.
      //
      if (receiverCommand[AUX2] > 1750 && previousAUX2 <= 1750)
        altitudeToHoldTarget += 4.0;
      else if (receiverCommand[AUX2] <= 1750 && previousAUX2 > 1750)
        altitudeToHoldTarget -= 4.0;
      previousAUX2 = receiverCommand[AUX2];
    #endif
  }
#endif


#if defined (AutoLanding)
  void processAutoLandingStateFromReceiverCommand() {
    if (receiverCommand[AUX3] < 1750) {
      if (altitudeHoldMode != ALT_PANIC ) {  // check for special condition with manditory override of Altitude hold
        if (!isAutoLandingInitialized) {
          autoLandingState = BARO_AUTO_DESCENT_STATE;
          autoLandingThrottleCorrection = 0;
          isAutoLandingInitialized = true;
        }
      }
    }
    else {
      autoLandingState = OFF;
      autoLandingThrottleCorrection = 0;
      isAutoLandingInitialized = false;
    }
  }
#endif


#if defined (UseGPSNavigator)
  void processGpsNavigationStateFromReceiverCommand() {
    // Init home command
    if (motorArmed == OFF && 
        receiverCommand[THROTTLE] < MINCHECK && receiverCommand[ZAXIS] < MINCHECK &&
        receiverCommand[YAXIS] > MAXCHECK && receiverCommand[XAXIS] > MAXCHECK &&
        haveAGpsLock()) {
  
      homePosition.latitude = currentPosition.latitude;
      homePosition.longitude = currentPosition.longitude;
      homePosition.altitude = DEFAULT_HOME_ALTITUDE;
    }


    if (receiverCommand[AUX2] < 1750) {  // Enter in execute mission state, if none, go back home, override the position hold
      if (!isGpsNavigationInitialized) {
        gpsRollAxisCorrection = 0;
        gpsPitchAxisCorrection = 0;
        gpsYawAxisCorrection = 0;
        isGpsNavigationInitialized = true;
      }
  
      positionHoldState = OFF;         // disable the position hold while navigating
      isPositionHoldInitialized = false;
  
      navigationState = ON;
    }
    else if (receiverCommand[AUX1] < 1250) {  // Enter in position hold state
      if (!isPositionHoldInitialized) {
        gpsRollAxisCorrection = 0;
        gpsPitchAxisCorrection = 0;
        gpsYawAxisCorrection = 0;
  
        positionHoldPointToReach.latitude = currentPosition.latitude;
        positionHoldPointToReach.longitude = currentPosition.longitude;
        positionHoldPointToReach.altitude = getBaroAltitude();
        isPositionHoldInitialized = true;
      }
  
      isGpsNavigationInitialized = false;  // disable navigation
      navigationState = OFF;
  
      positionHoldState = ON;
    }
    else {
      // Navigation and position hold are disabled
      positionHoldState = OFF;
      isPositionHoldInitialized = false;
  
      navigationState = OFF;
      isGpsNavigationInitialized = false;
  
      gpsRollAxisCorrection = 0;
      gpsPitchAxisCorrection = 0;
      gpsYawAxisCorrection = 0;
    }
  }
#endif




void processZeroThrottleFunctionFromReceiverCommand() {
  // Disarm motors (left stick lower left corner)
  if (receiverCommand[ZAXIS] < MINCHECK && motorArmed == ON) {
    commandAllMotors(MINCOMMAND);
    motorArmed = OFF;
    inFlight = false;

    #ifdef OSD
      notifyOSD(OSD_CENTER|OSD_WARN, "MOTORS UNARMED");
    #endif

    #if defined BattMonitorAutoDescent
      batteryMonitorAlarmCounter = 0;
      batteryMonitorStartThrottle = 0;
      batteyMonitorThrottleCorrection = 0.0;
    #endif
  }    

  // Zero Gyro and Accel sensors (left stick lower left, right stick lower right corner)
  if ((receiverCommand[ZAXIS] < MINCHECK) && (receiverCommand[XAXIS] > MAXCHECK) && (receiverCommand[YAXIS] < MINCHECK)) {
    calibrateGyro();
    computeAccelBias();
    storeSensorsZeroToEEPROM();
    calibrateKinematics();
    zeroIntegralError();
    pulseMotors(3);
  }   

  // Arm motors (left stick lower right corner)
  if (receiverCommand[ZAXIS] > MAXCHECK && motorArmed == OFF && safetyCheck == ON) {

    #ifdef OSD_SYSTEM_MENU
      if (menuOwnsSticks) {
        return;
      }
    #endif

    for (byte motor = 0; motor < LASTMOTOR; motor++) {
      motorCommand[motor] = MINTHROTTLE;
    }
    motorArmed = ON;

    #ifdef OSD
      notifyOSD(OSD_CENTER|OSD_WARN, "!MOTORS ARMED!");
    #endif  

    zeroIntegralError();
    resetAltitude();
  }
  // Prevents accidental arming of motor output if no transmitter command received
  if (receiverCommand[ZAXIS] > MINCHECK) {
    safetyCheck = ON; 
  }
}




/**
 * readPilotCommands
 * 
 * This function is responsible to read receiver
 * and process command from the users
 */
void readPilotCommands() {

  readReceiver(); 
  
  if (receiverCommand[THROTTLE] < MINCHECK) {
    processZeroThrottleFunctionFromReceiverCommand();
  }

  if (!inFlight) {
    if (motorArmed == ON && receiverCommand[THROTTLE] > minArmedThrottle) {
      inFlight = true;
    }
  }

    // Check Mode switch for Acro or Stable
    if (receiverCommand[MODE] > 1500) {
        flightMode = ATTITUDE_FLIGHT_MODE;
    }
    else {
        flightMode = RATE_FLIGHT_MODE;
    }
    
    if (previousFlightMode != flightMode) {
      zeroIntegralError();
      previousFlightMode = flightMode;
    }


  #if defined AltitudeHoldBaro || defined AltitudeHoldRangeFinder
    processAltitudeHoldStateFromReceiverCommand();
  #endif
  
  #if defined (AutoLanding)
    processAutoLandingStateFromReceiverCommand();
  #endif

  #if defined (UseGPSNavigator)
    processGpsNavigationStateFromReceiverCommand();
  #endif
}

#endif // _AQ_FLIGHT_COMMAND_READER_
