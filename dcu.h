#ifndef _DASHCONTROLUNIT_H
#define _DASHCONTROLUNIT_H

#include <stdio.h>
#include <stdint.h>

#include "serial.h"
#include "IO_CAN.h"


typedef struct _DashControlUnit DashControlUnit;

/**
* @brief DCU object constructor
*
* @return      dcu object
*/
DashControlUnit* DCU_new(void);


/**
* @brief Handles the data in a CAN message from the DCU
*
* @param[in]   me	       Pointer to the dcu object
* @param[in]   canMessage  The message to be parsed
*/
void DCU_parseCanMessage(DashControlUnit* me, IO_CAN_DATA_FRAME* canMessage);



#endif // _DASHCONTROLUNIT_H


