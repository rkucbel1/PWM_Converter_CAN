/*******************************************************************************
 System Tasks File

  File Name:
    tasks.c

  Summary:
    This file contains source code necessary to maintain system's polled tasks.

  Description:
    This file contains source code necessary to maintain system's polled tasks.
    It implements the "SYS_Tasks" function that calls the individual "Tasks"
    functions for all polled MPLAB Harmony modules in the system.

  Remarks:
    This file requires access to the systemObjects global data structure that
    contains the object handles to all MPLAB Harmony module objects executing
    polled in the system.  These handles are passed into the individual module
    "Tasks" functions to identify the instance of the module to maintain.
 *******************************************************************************/

// DOM-IGNORE-BEGIN
/*******************************************************************************
* Copyright (C) 2018 Microchip Technology Inc. and its subsidiaries.
*
* Subject to your compliance with these terms, you may use Microchip software
* and any derivatives exclusively with Microchip products. It is your
* responsibility to comply with third party license terms applicable to your
* use of third party software (including open source software) that may
* accompany Microchip software.
*
* THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER
* EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY IMPLIED
* WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS FOR A
* PARTICULAR PURPOSE.
*
* IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE,
* INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND
* WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS
* BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO THE
* FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL CLAIMS IN
* ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF FEES, IF ANY,
* THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.
 *******************************************************************************/
// DOM-IGNORE-END

// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************

#include "configuration.h"
#include "definitions.h"
#include "GNSStasks.h"

QueueHandle_t xUBXqueue;
SemaphoreHandle_t xUBXMutex;

TaskHandle_t xTaskGetUART1bytes;

static void lvTaskGetUART1bytes(void *pvParameters)
{   
  while(1) {
    vTaskGetUART1bytes();
  }
}

TaskHandle_t xTaskProcessUBXmessage;

static void lvTaskProcessUBXmessage(void *pvParameters)
{   
  while(1) {
    vTaskProcessUBXmessage();
  }
}

TaskHandle_t xTaskSendUBXmessage;

static void lvTaskSendUBXmessage(void *pvParameters)
{   
  while(1) {
    vTaskSendUBXmessage();
  }
}

TaskHandle_t xTaskSendUBXheartbeat;

static void lvTaskSendUBXheartbeat(void *pvParameters)
{   
  while(1) {
    vTaskSendUBXheartbeat();
  }
}


/*******************************************************************************
  Function:
    void SYS_Tasks ( void )

  Remarks:
    See prototype in system/common/sys_module.h.
*/
void SYS_Tasks ( void )
{
  xUBXqueue = xQueueCreate(64, sizeof(uint8_t));
  
  xUBXMutex = xSemaphoreCreateMutex();
  
  if (xUBXMutex == NULL){
    /* The semaphore could not be created and program will run forever here */
    while(1); 
  }
  
  if(xUBXqueue != NULL){
  
    (void) xTaskCreate((TaskFunction_t)lvTaskGetUART1bytes,      "GetUART1bytes",    1024, NULL, 1, &xTaskGetUART1bytes);
    (void) xTaskCreate((TaskFunction_t)lvTaskProcessUBXmessage, "ProcessUBXmessage", 1024, NULL, 2, &xTaskProcessUBXmessage);
    (void) xTaskCreate((TaskFunction_t)lvTaskSendUBXmessage,    "SendUBXmessage",    1024, NULL, 3, &xTaskSendUBXmessage);
    (void) xTaskCreate((TaskFunction_t)lvTaskSendUBXheartbeat,  "SendUBXheartbeat",   512, NULL, 3, &xTaskSendUBXheartbeat);
    
    vTaskStartScheduler();
  }
  else {
    /* The queue could not be created */
  }
  
}

/*******************************************************************************
 End of File
 */

