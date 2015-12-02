/**********************************************************************//**
 * \file sensors.h
 *
 * \brief Sensor object definitions and basic functions
 *
 *      The IO Driver high level interface provides a general
 *      initialization function, a version API and general task functions
 *      which shall wrap the whole user application.
 *
 * \section appl_structure Basic structure of an application
 *
 *      The IO Driver API provides two different types of functions:
 *      - Initialization functions:
 *              These functions are designed to be called once at the
 *              beginning of an application.
 *      - Task functions:
 *              These functions are designed to be called periodically at
 *              runtime.
 *
 *      The function IO_Driver_Init() needs to be the first function, called
 *      during the initialization.
 *
 *      All task functions need to be enclosed by the functions
 *      IO_Driver_TaskBegin() and IO_Driver_TaskEnd()
 *
 *      Example of an application:
 *
 *      \code
 *      void task (void)
 *      {
 *          IO_Driver_TaskBegin();
 *
 *          // User Application
 *          // and calls to driver task functions.
 *
 *          IO_Driver_TaskEnd();
 *      }
 *
 *
 *      void main (void)
 *      {
 *          ubyte4 timestamp
 *
 *          //--------------------------------//
 *          // start of driver initialization //
 *          //--------------------------------//
 *
 *          // IO_Driver_Init() is the first function:
 *          IO_Driver_Init(NULL);  // no safety critical application
 *
 *          //------------------------------//
 *          // end of driver initialization //
 *          //------------------------------//
 *
 *
 *          //--------------------------------------------//
 *          // from now on only task functions are called //
 *          //--------------------------------------------//
 *          while (1){
 *              IO_RTC_StartTime(&timestamp);
 *
 *              task();
 *
 *              while (IO_RTC_GetTimeUS(timestamp) < 5000);
 *          }
 *      }
 *
 *      \endcode
 *
 *      The \c task function is called every 5000us = 5ms. Please
 *      refer to the \ref IO_RTC.h "Real Time Clock" documentation for
 *      details on how to use the RTC functions.
 *
 * \section startup_limits Limitations during startup
 *         During the first cycle the measurement values (eg. ADC, PWD, etc.) are
 *         invalid, which is indicated by the corresponding fresh flag when reading the value.
 *         Starting with the second cycle the actual measurement values are available.
 *
 *         Further limitations for safety controllers:
 *         The safety controllers (\safetycontrollers) perform various tests at startup.
 *         For this reason the measurement values can hold unexpected values during this process.
 *         The application software should wait until the CPU has entered
 *         the Main state before evaluating the input variables.
 *         For details on CPU states and how to retrieve the current state,
 *         refer to \ref DIAG_Functions.h "Functions for ECU diagnostics"
 *
 * \section general_limits General limitations
 *         When the IO Driver is configured as safety critical(safety controllers \safetycontrollers),
 *         all the variables modified by an ISR shall be placed in internal RAM only, otherwise a
 *         safety state could be triggered during the execution of the External RAM Memory check.
 *
 **************************************************************************/


//"Include guard" - prevents this file from being #included more than once
#ifndef _SENSORS_H
#define _SENSORS_H

//----------------------------------------------------------------------------
// Sensor Object Properties
//----------------------------------------------------------------------------
// canMessageBaseId - The starting CAN address for all messages
ubyte1 canMessageBaseId = 0x500;


//----------------------------------------------------------------------------
// Sensor Object Definitions
//----------------------------------------------------------------------------
// Parameters:
// canMessageIdOffset - Unique ID (address) for each CAN message, starting at
//                      CANBaseAddress.  Multiple sensors
//                can be assigned to a single ID/message if they'll fit in the
//                available data space
//                TODO: What's the limit?
//				  https://docs.google.com/spreadsheets/d/1sYXx191RtMq5Vp5PbPsq3BziWvESF9arZhEjYUMFO3Y/edit
// TODO: Write the rest of these
//
// specMin/Max values should come from each sensor's datasheets, but it is not
// required for all sensors.
//
// TODO: What about having default calbiration values?  (Probably useless)
//----------------------------------------------------------------------------
typedef struct _Sensor {
    //CAN stuff
    ubyte1 canMessageIdOffset;  //Required!
    //Note: CAN message address = canMessageBaseId + canMessageIdOffset

    //Sensor values / properties
    ubyte2 specMin;
    ubyte2 specMax;
    bool isCalibrated;
    ubyte2 calibMin;
    ubyte2 calibMax;
    ubyte2 calibNormal;  //zero value or normal position

    ubyte2 sensorValue;
    bool fresh;

    ubyte2 calibratedValue;

} Sensor;

//----------------------------------------------------------------------------
// Sensor Object Instantiations
//----------------------------------------------------------------------------
// canMessageIdOffset should be specified NOW!
//----------------------------------------------------------------------------

//TODO: Read stored calibration data from EEPROM

//Torque Encoders (TPS is not really accurate since there's no throttle to position in an EV)
Sensor Sensor_TPS0 = { 0, 0.5, 4.5 };
Sensor Sensor_TPS1 = { 0, 4.5, 0.5 };

//Brake Position Sensors
Sensor Sensor_BPS0 = { 1, 0.5, 4.5 };  //Brake system pressure (or front only in the future)
                                       //Sensor Sensor_BPS1 = { 2, 0.5, 4.5 }; //Rear brake system pressure (separate address in case used for something else)

                                       //Wheel Speed Sensors (like an ABS sensor)
Sensor Sensor_WSS_FL = { 3 };
Sensor Sensor_WSS_FR = { 3 };
Sensor Sensor_WSS_RL = { 3 };
Sensor Sensor_WSS_RR = { 3 };

//Wheel Position Sensors (Shock pots)
Sensor Sensor_WPS_FL = { 4 };
Sensor Sensor_WPS_FR = { 4 };
Sensor Sensor_WPS_RL = { 4 };
Sensor Sensor_WPS_RR = { 4 };

//Switches
//precharge failure

//Other
Sensor Sensor_LVBattery = { 0xA };  //Note: There will be no init for this "sensor"


//----------------------------------------------------------------------------
// Sensor Functions
//----------------------------------------------------------------------------


#endif /* _SENSORS_H */
