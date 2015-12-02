/*****************************************************************************
* Output Calculations
******************************************************************************
* Takes properties from devices (such as raw sensor values [ohms, voltage], 
* MCU/BMS CAN messages, etc), performs calculations with that data, and updates
* the relevant objects' properties.
*
* This includes sensor calculations, motor controller control calculations,
* traction control, BMS/safety calculations, etc.
* (May need to split this up later)
*
* For example: GetThrottlePosition() takes the raw TPS voltages from the TPS 
* sensor objects and returns the throttle pedal percent.  This function does 
* NOT update the sensor objects, but it would be acceptable for another 
* function in this file to do so.
*
******************************************************************************
* To-do:
*
******************************************************************************
* Revision history:
* 2015-11-16 - Rusty Pedrosa - 
*****************************************************************************/

//VCU/C headers
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "IO_Driver.h"  //Includes datatypes, constants, etc - should be included in every c file

#include "sensors.h"

/*****************************************************************************
* Helper functions
****************************************************************************/
/*-------------------------------------------------------------------
* getPercent
* Returns the % (position) of value, between min and max
* If zeroToOneOnly is true, then % will be capped at 0%-100% (no negative % or > 100%)
-------------------------------------------------------------------*/
float4 getPercent(float4 value, float4 min, float4 max, bool zeroToOneOnly)
{
    float4 retVal = (value - min) / (max - min);

    if (zeroToOneOnly == TRUE)
    {
        if (retVal > 1) { retVal = 1; }
        else if (retVal < 0) { retVal = 0; }
    }

    return retVal;
}

/*****************************************************************************
* Torque Encoder (TPS) functions
* RULE EV2.3.5:
* If an implausibility occurs between the values of these two sensors the power to the motor(s) must be immediately shut down completely.
* It is not necessary to completely deactivate the tractive system, the motor controller(s) shutting down the power to the motor(s) is sufficient.
****************************************************************************/

// Physical pedal travel will only occur across the center (about 1/2) of the actual sensor's range of travel
// The rules (especially EV2.3.6) are written about % of PEDAL travel, not percent of sensor range, so we must calculate pedal travel by recording the min/max voltages at min/max throttle positions

/*-------------------------------------------------------------------
* CalibrateTPS
* Description: Records TPS minimum/maximum voltages (when?) and stores them (where?)
* Parameters:
* Inputs:
* Returns:
* Notes:
* Throws:
-------------------------------------------------------------------*/
void calibrateTPS(ubyte1 secondsToRunCalibration)
{
    //TODO: Make sure the main loop is running before doing this

    //Reset calibration data
    Sensor_TPS0.calibMin = Sensor_TPS0.specMax;
    Sensor_TPS0.calibMax = Sensor_TPS0.specMin;
    Sensor_TPS0.isCalibrated = FALSE;
    Sensor_TPS1.calibMin = Sensor_TPS1.specMax;
    Sensor_TPS1.calibMax = Sensor_TPS1.specMin;
    Sensor_TPS1.isCalibrated = FALSE;

    ubyte4 timestamp_calibrationstart = 0;
    IO_RTC_StartTime(&timestamp_calibrationstart);  //Get a timestamp of when this task started from the Real Time Clock
                                                    //TODO: Don't start coundown until values have been set
    while (IO_RTC_GetTimeUS(timestamp_calibrationstart) < (ubyte4)secondsToRunCalibration * 1000 * 1000)   // Give the user # seconds to do this
    {
        //TODO: Do something on the display to show that voltages are being recorded
        //Idea: Display "bars" filling up on right segment (for gas pedal) _=E=_=E...
        //      Once calibration data makes sense, show pedal location (0-10%, 10-90%, 90-100%) with bars

        //Record min/max voltages
        if (Sensor_TPS0.sensorValue < Sensor_TPS0.calibMin) {
            Sensor_TPS0.calibMin = Sensor_TPS0.sensorValue;
        }
        if (Sensor_TPS0.sensorValue > Sensor_TPS0.calibMax) {
            Sensor_TPS0.calibMax = Sensor_TPS0.sensorValue;
        }
        if (Sensor_TPS1.sensorValue < Sensor_TPS1.calibMin) {
            Sensor_TPS1.calibMin = Sensor_TPS1.sensorValue;
        }
        if (Sensor_TPS1.sensorValue > Sensor_TPS1.calibMax) {
            Sensor_TPS1.calibMax = Sensor_TPS1.sensorValue;
        }

    }

    //TODO: Write calibration data to EEPROM

    //TODO: Check for valid/reasonable calibration data
    Sensor_TPS0.isCalibrated = TRUE;
    Sensor_TPS1.isCalibrated = TRUE;

    //TODO: Return the display to normal
}

/*-------------------------------------------------------------------
* GetThrottlePosition
* Description: Reads TPS Pin voltages and returns % of throttle pedal travel.
* Parameters:  None
* Inputs:      Assumes TPS#.sensorValue has been set by main loop
* Returns:     Throttle value in percent (from 0 to 1)
* Notes:       Valid pedal travel is from 10% (0.10) to 90% (0.90), not including mechanical limits.
* Throws:      000 - TPS0 voltage out of range
*              001 - TPS1 voltage out of range, 002
-------------------------------------------------------------------*/
float4 GetThrottlePosition(void)
{
    //TODO: Should we calculate this during the main loop? (probably not - what if it's not calibrated?)
    //      or should we only run this when someone specifically asks for throttle position? (probably)

    ubyte1 TPSErrorState = 0; //No errors have been detected so far.

                              //-------------------------------------------------------------------
                              //First check for implausibility at the pin level
                              //USE SPEC SHEET VALUES, NOT CALIBRATION VALUES
                              //RULE: EV2.3.10 - signal outside of operating range is considered a failure
                              //Note: IC cars may continue to drive for up to 100ms until valid readings are restored, but EVs must immediately cut power
                              //Note: We need to decide how to report errors and how to perform actions when those errors occur.  For now, I'm calling an imaginary Err.Report function
                              //-------------------------------------------------------------------
    if ((Sensor_TPS0.sensorValue < Sensor_TPS0.specMin) || (Sensor_TPS0.sensorValue > Sensor_TPS0.specMax))
    {
        //TODO: Err.Report(Err.Codes.TPS0Range, "TPS0 out of range:" & TPS0.sensorValue, Motor.Disable);
        //Note: We want to continue to run the rest of the code in this function to detect additional errors which we can report back to the dash.  To prevent the rest of the code from applying throttle, we take note that an error has occurred
        TPSErrorState++;
    }

    if ((Sensor_TPS1.sensorValue < Sensor_TPS1.specMin) || (Sensor_TPS1.sensorValue > Sensor_TPS1.specMax))
    {
        //TODO: Err.Report(Err.Codes.TPS1Range, "TPS1 out of range" & Pin140.sensorValue, Motor.Disable);
        TPSErrorState++;

    }

    //-------------------------------------------------------------------
    // If there are no issues at the pin level, then the next step
    // is to make sure the sensors actually agree with each other
    //-------------------------------------------------------------------
    //Calculate individual throttle percentages
    //Percent = (Voltage - CalibMin) / (CalibMax - CalibMin)
    float4 TPS0PedalPercent = getPercent(Sensor_TPS0.sensorValue, Sensor_TPS0.calibMin, Sensor_TPS0.calibMax, FALSE); //Analog Input 0
    float4 TPS1PedalPercent = getPercent(Sensor_TPS1.sensorValue, Sensor_TPS1.calibMin, Sensor_TPS1.calibMax, FALSE); //Analog input 1

                                                                                                                      //Check for implausibility (discrepancy > 10%)
                                                                                                                      //RULE: EV2.3.6 Implausibility is defined as a deviation of more than 10% pedal travel between the sensors.
    if (fabs(TPS1PedalPercent - TPS0PedalPercent) > .1)
    {
        //Err.Report(Err.Codes.TPSDiscrepancy, "TPS discrepancy of over 10%", Motor.Stop);
        TPSErrorState++;
    }

    //-------------------------------------------------------------------
    //Make sure the sensors have been calibrated
    //-------------------------------------------------------------------
    if ((Sensor_TPS0.isCalibrated == FALSE) || (Sensor_TPS1.isCalibrated == FALSE))
    {
        TPSErrorState++;
    }

    //What about other error states?
    //Voltage outside of calibration range
    //Voltages off center

    if (TPSErrorState > 0)
    {
        return 0;
    }
    else
    {
        return (TPS0PedalPercent + TPS1PedalPercent) / 2;
    }
}



/*****************************************************************************
* BPS (Brake Pressure Sensor) functions
*
****************************************************************************/
//Input: Voltage
//Outputs: ???

/*-------------------------------------------------------------------
* CalibrateBPS
* Description: Records brake system pressure at point where wheels
*              lock up [THIS IS A SUGGESTION - maybe we just use the
*              DAQ to determine this and hardcode a value
* Parameters:
* Inputs:      Pin151 = Analog Input 2
* Returns:
* Notes:
* Throws:
-------------------------------------------------------------------*/
void CalibrateBPS(void) {
    //TODO: Figure out how much braking effort (in hydraulic system PSI) = 100%
    //      braking effort (shooting for 100 lbs pedal effort?)
    //TODO: Use above value for 100% of "pedal travel"
    //      For now, assume 4.5V = 100 lbs 
    //TODO: Decide how often this must be done.  Could resting pressure vary
    //      with ambient temperature enough to mess up regen levels?
    //Note: It may not be feasible to measure max pressure that each driver will apply.  Instead, it may be best to go with absolute values (.5 to 4.5 V), or to at least record PSI or Voltage at zero pedal.  Not sure how this is affected by having a Sealed sensor (as opposed to Gage), but presumably brake force is SIGNIFICANTLY higher than ambient temperature anyway.
    //Sensor_BPS0.CalibMin = Sensor_BPS0.GetVoltage.Min;
    //Sensor_BPS0.CalibMax = Sensor_BPS0.GetVoltage.Max;
}

/*-------------------------------------------------------------------
* GetBrakePosition
* Description: Reads TPS Pin voltages and returns % of throttle pedal travel.
* Parameters:  None
* Inputs:
* Returns:     Brake pedal travel in percent (from 0 to 1), where
*              0 = no pedal travel and 100 = //TODO: 4 locked wheels???
*              NOTE: This means values above 100% are both allowed
*                    and EXPECTED during e.g. brake tests & extreme lockups
* Notes:
* Throws:
-------------------------------------------------------------------*/
void GetBrakePosition(void)
{
    //GetTPSValue = 0;  //This is the default value that this function will return
    //TPSErrorState = 0; //No errors have been detected so far.
    //-------------------------------------------------------------------
    //First check for implausibility at the pin level
    //USE SPEC SHEET VALUES, NOT CALIBRATION VALUES
    //RULE: EV2.4.4 - signal outside of operating range (EV2.4.5) requires
    //                immediate shutdown of power to motor
    //-------------------------------------------------------------------
    //if(Sensor_BPS.GetVoltage < Sensor_BPS.SpecMin Or Sensor_BPS.GetVoltage > Sensor_BPS.SpecMax
    {
        //Err.Report(Err.Codes.BPSRange, "BPS voltage out of range:" & Sensor_BPS.GetVoltage, Motor.Disable);
        //TPSErrorState++;
    }

    //-------------------------------------------------------------------
    // Calculate pedal travel
    //-------------------------------------------------------------------
    //Sensor_BPS.PedalPercent = Sensor_BPS.GetVoltage / Sensor_BPS.CalibMax;

    //If TPSErrorState > 0 Then
    //Reduce motor torque to zero
    //GetTPSValue already = 0;
    //Else
    //GetBrakePosition = Sensor_BPS.PedalPercent;
    //End If
}


/*****************************************************************************
* Wheel Speed Sensor functions
* FR = Pin275 = Digital Timer Input 0
* FL = Pin268 = Digital Timer Input 1
* RR = Pin274 = Digital Timer Input 2
* RL = Pin267 = Digital Timer Input 3
****************************************************************************/
//Inputs: Wheel speed sensor frequency
//Outputs: 4 speeds, RPM, ground speed
//4 funcs, one to get each wheel�s speed
//16 bumps per rotation, 16 hz = 1 rotation per second
#define NUM_BUMPS 16
#define WHEEL_DIAMETER 18 //Inches
#define NUM_BUMPS_D (double)NUM_BUMPS
#define WHEEL_DIAMETER_D (double)WHEEL_DIAMETER

double freq_to_rpm(double freq) {
    return (double)((freq / NUM_BUMPS_D)*60.0);
}

//Theoretical ground speed
//63360 inches per mile. Wish we could use metric.
double rpm_to_mph(double rpm) {
    return (double)((3.14159265*WHEEL_DIAMETER_D*rpm*60.0) / 63360.0);
}


/*****************************************************************************
* Shock pot(iometer) functions - FOUR NEEDED
* FR = Pin150 = Analog Input 4
* FL = Pin138 = Analog Input 5
* RR = Pin149 = Analog Input 6
* RL = Pin137 = Analog Input 7
* 0 = ride height
****************************************************************************/
//Input: Ohms
//Outputs: ???
//See VCU Manual section 5.8.8 - there are different outputs depending on sensor resistance
//Example sensor: Active Sensors CLS0950
//Resistive range: 0.4 to 6.0 kohm
//ShockPot.


/*****************************************************************************
* Doppler speed sensor functions
*
****************************************************************************/
//Input: Frequency
//Outputs: ???

/*****************************************************************************
* Water temp sensor functions
*
****************************************************************************/
//Input: Ohms
//Outputs: ???

/*****************************************************************************
* Dash button functions
*
****************************************************************************/
//TCS
//ECO
//RTD
//blablalba



/*****************************************************************************
* Error Reporting Stuff
*
* TODO: Write error reporting functions
****************************************************************************/
//Err.Report(ErrCode, ErrDesc, ErrAction) {
//Write an error to the log
//Add error code to dash
//000 = TPS0 out of range
//001 = TPS1 out of range
//}

