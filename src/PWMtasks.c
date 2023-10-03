/******************************************************************************/
/* GNSStasks.c                                                                */
/*                                                                            */
/* Author: Roman "BEEF" Kucbel                                                */
/* Date: 3-Sep-2023                                                           */
/*                                                                            */
/******************************************************************************/

#include "GNSStasks.h"


#define UBX_FRAME_SOL      0x06
#define UBX_FRAME_PVT      0x07
#define UBX_FRAME_TIMEGPS  0x20
#define UBX_FRAME_COV      0x36

#define UBX_OFFSET 4

/* Buffer used to receive from DMA*/
static uint8_t __attribute__((coherent)) readByte[8] = {};

/* Buffer used to hold received UBX frame for further processing (packing into PDUs) */
uint8_t UBX_BUF[100];

/* Max Time out waiting for FreeRTOS kernel objects */
TickType_t timeout = 1000/portTICK_PERIOD_MS;

/* Structures used to hold parts of interest of each UBX Frame. */
/* Each structure contains members that are CAN messages (PDUs) packed according to a dbc file description. */
/* TIMEGPS is bundled into PVT */
struct UBX_SOLstruct {
  
  uint8_t NavSOL_PartA[8]; /*ecefX and ecefY */
  uint8_t NavSOL_PartB[8]; /*ecefZ and ecefVX */
  uint8_t NavSOL_PartC[8]; /*eceVY and eceVZ */
  uint8_t f_NavSOL;
    
} PDU_sol;

struct UBX_PVTstruct {
  
  uint8_t NavPVT_Time[8]; /* YY, MM, DD, HR, MIN, SEC, leapSec */
  uint8_t NavPVT_Misc[8]; /* FixType, numSV, VNEDD, PDOP */
  uint8_t NavPVT_LatLon[8]; /* Lon, Lat */
  uint8_t NavPVT_Height[8]; /* height, heightMSL */
  uint8_t NavPVT_VNED[8]; /* VNEDN, VNEDE */
  uint8_t f_NavPVT;
    
} PDU_pvt;

struct UBX_COVstruct {
  
  uint8_t NavCOV_PartA[8]; /* posCOVNN, posCOVNE */
  uint8_t NavCOV_PartB[8]; /* posCOVND, velCOVNN */
  uint8_t NavCOV_PartC[8]; /* velCOVNE, velCOVND */
  uint8_t f_NavCOV;
    
} PDU_cov;

/* Used for testing */
#define UBX_BUF_TEST_SIZE 96

/*UBX_TIMEGPS UBX_BUF_TEST_SIZE = 24*/
//uint8_t testArray[] = {0xB5, 0x62, 0x01, 0x20, 0x10, 0x00, 0xD8, 0x7D, 0x13, 0x00,
//                       0xDE, 0xAD, 0xBE, 0xEF, 0xA5, 0xA5, 0xA5, 0xA5, 0xFE, 0xED,
//                       0xBE, 0xEF, 0xA6, 0xCD};

/*UBX_SOL UBX_BUF_TEST_SIZE = 60*/
//uint8_t testArray[] = {0xB5, 0x62, 0x01, 0x06, 0x10, 0x00, 0xD8, 0x7D, 0x13, 0x00,
//                       0xDE, 0xAD, 0xBE, 0xEF, 0xA5, 0xA5, 0xA5, 0xA5, 0xFE, 0xED,
//                       0xDE, 0xAD, 0xBE, 0xEF, 0xA5, 0xA5, 0xA5, 0xA5, 0xFE, 0xED,
//                       0xDE, 0xAD, 0xBE, 0xEF, 0xA5, 0xA5, 0xA5, 0xA5, 0xFE, 0xED,
//                       0xDE, 0xAD, 0xBE, 0xEF, 0xA5, 0xA5, 0xA5, 0xA5, 0xFE, 0xED,
//                       0xDE, 0xAD, 0xBE, 0xEF, 0xA5, 0xA5, 0xBE, 0xEF, 0xA6, 0xCD};

/*UBX_COV UBX_BUF_TEST_SIZE = 72*/
//uint8_t testArray[] = {0xB5, 0x62, 0x01, 0x36, 0x10, 0x00, 0xD8, 0x7D, 0x13, 0x00,
//                       0xDE, 0xAD, 0xBE, 0xEF, 0xA5, 0xA5, 0xA5, 0xA5, 0xFE, 0xED,
//                       0xDE, 0xAD, 0xBE, 0xEF, 0xA5, 0xA5, 0xA5, 0xA5, 0xFE, 0xED,
//                       0xDE, 0xAD, 0xBE, 0xEF, 0xA5, 0xA5, 0xA5, 0xA5, 0xFE, 0xED,
//                       0xDE, 0xAD, 0xBE, 0xEF, 0xA5, 0xA5, 0xA5, 0xA5, 0xFE, 0xED,
//                       0xDE, 0xAD, 0xBE, 0xEF, 0xA5, 0xA5, 0xA5, 0xA5, 0xFE, 0xED,
//                       0xDE, 0xAD, 0xBE, 0xEF, 0xA5, 0xA5, 0xA5, 0xA5, 0xFE, 0xED,
//                       0xA6, 0xCD};

/*UBX_PVT UBX_BUF_TEST_SIZE = 96*/
//uint8_t testArray[] = {0xB5, 0x62, 0x01, 0x07, 0x10, 0x00, 0xD8, 0x7D, 0x13, 0x00,
//                       0xDE, 0xAD, 0xBE, 0xEF, 0xA5, 0xA5, 0xA5, 0xA5, 0xFE, 0xED,
//                       0xDE, 0xAD, 0xBE, 0xEF, 0xA5, 0xA5, 0xA5, 0xA5, 0xFE, 0xED,
//                       0xDE, 0xAD, 0xBE, 0xEF, 0xA5, 0xA5, 0xA5, 0xA5, 0xFE, 0xED,
//                       0xDE, 0xAD, 0xBE, 0xEF, 0xA5, 0xA5, 0xA5, 0xA5, 0xFE, 0xED,
//                       0xDE, 0xAD, 0xBE, 0xEF, 0xA5, 0xA5, 0xA5, 0xA5, 0xFE, 0xED,
//                       0xDE, 0xAD, 0xBE, 0xEF, 0xA5, 0xA5, 0xA5, 0xA5, 0xFE, 0xED,
//                       0xDE, 0xAD, 0xBE, 0xEF, 0xA5, 0xA5, 0xA5, 0xA5, 0xFE, 0xED,
//                       0xDE, 0xAD, 0xBE, 0xEF, 0xA5, 0xA5, 0xA5, 0xA5, 0xFE, 0xED,
//                       0xFE, 0xED, 0xBE, 0xEF, 0xA6, 0xCD};


static void UARTDmaChannelHandler(DMAC_TRANSFER_EVENT event, uintptr_t contextHandle)
{
    BaseType_t xHigherPriorityTaskWoken;
    
    xHigherPriorityTaskWoken = pdFALSE;
    
    if (event == DMAC_TRANSFER_EVENT_COMPLETE) {
      vTaskNotifyGiveFromISR(xTaskGetUART1bytes, &xHigherPriorityTaskWoken);
    }
}

void vTaskGetUART1bytes(void)
{
   uint8_t i;
  
   /* Block until DMA is done */
   ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
   
   /* When DMA done, copy data from readByte[] into xUBXqueue */
   for(i=0; i<8; i++) { 
     xQueueSendToBack(xUBXqueue, &readByte[i], 0);
   }
   
   /* Used for testing */
   //for(i=0; i<UBX_BUF_TEST_SIZE; i++) { 
   //  xQueueSendToBack(xUBXqueue, &testArray[i], 0);
   //}
}

void vTaskProcessUBXmessage(void)
{
  uint8_t ReceivedValue = 0x00;
  BaseType_t xStatus;
  
  static uint8_t f_FrameFoundHeader = 0;
  uint8_t f_FrameProcessed;
  uint8_t FrameType;
   
  xStatus = xQueueReceive(xUBXqueue, &ReceivedValue, portMAX_DELAY);
  
  if( xStatus == pdPASS ) {
      
    /* Look for the header of a UBX frame. Look for byte sequence 0xB5 0x62 0x01 0xXX where 0xXX is type */
    if (f_FrameFoundHeader != 4) {
      f_FrameFoundHeader = UBXframeFind(ReceivedValue, &FrameType);
    }
    
    else if ((f_FrameFoundHeader == 1) || (f_FrameFoundHeader == 2) || (f_FrameFoundHeader == 3)) {
      /* Frame Header is being processed, but don't need to do anything this time through */
    }
      
    /* If a header has been found, populate UBX_BUF */
   else if (f_FrameFoundHeader == 4) {
     f_FrameProcessed = UBXframeProcess(ReceivedValue, FrameType);
       
      /* If UBX_BUF is populated, pack the appropriate PDU structures */
     if (f_FrameProcessed == 3) {
       if(xSemaphoreTake(xUBXMutex, portMAX_DELAY) == pdTRUE) {
         PackPDU(FrameType);
         xSemaphoreGive(xUBXMutex);
         f_FrameFoundHeader = 0;
       }
       else {
         /* Timeout occurred - implement reset */
           while(1); //this is temporary for debugging
       }
     }
         
     else {
       /* Do Nothing */
     }
    }
      
    else {
      f_FrameProcessed = 0;
    }
  }
  
  else {
    /* Do Nothing */    
  }
}

void vTaskSendUBXmessage(void)
{
  
  if (PDU_pvt.f_NavPVT == 1) {
    if (xSemaphoreTake(xUBXMutex, timeout) == pdTRUE) {
      taskENTER_CRITICAL();
      if (CAN1_MessageTransmit(0x470, 8, &PDU_pvt.NavPVT_Time[0], 1, 0, 0) == true) { HB_LED_Toggle(); }
      if (CAN1_MessageTransmit(0x471, 8, &PDU_pvt.NavPVT_Misc[0], 1, 0, 0) == true) { HB_LED_Toggle(); }
      if (CAN1_MessageTransmit(0x472, 8, &PDU_pvt.NavPVT_LatLon[0], 1, 0, 0) == true) { HB_LED_Toggle(); }
      if (CAN1_MessageTransmit(0x473, 8, &PDU_pvt.NavPVT_Height[0], 1, 0, 0) == true) { HB_LED_Toggle(); }
      if (CAN1_MessageTransmit(0x474, 8, &PDU_pvt.NavPVT_VNED[0], 1, 0, 0) == true) { HB_LED_Toggle(); }
      WDT_Clear();
      taskEXIT_CRITICAL();
    
      PDU_pvt.f_NavPVT = 0;
    
      xSemaphoreGive(xUBXMutex);
    }
    else {
      /* Timeout occurred - implement SW reset*/
      while(1);
    }
  }
  
   if (PDU_cov.f_NavCOV == 1) {
     if (xSemaphoreTake(xUBXMutex, timeout) == pdTRUE) {
       taskENTER_CRITICAL();
       if (CAN1_MessageTransmit(0x475, 8, &PDU_cov.NavCOV_PartA[0], 1, 0, 0) == true) { HB_LED_Toggle(); }
       if (CAN1_MessageTransmit(0x476, 8, &PDU_cov.NavCOV_PartB[0], 1, 0, 0) == true) { HB_LED_Toggle(); }
       if (CAN1_MessageTransmit(0x477, 8, &PDU_cov.NavCOV_PartC[0], 1, 0, 0) == true) { HB_LED_Toggle(); }
       WDT_Clear();
       taskEXIT_CRITICAL();
   
       PDU_cov.f_NavCOV = 0;
  
       xSemaphoreGive(xUBXMutex);
   }
     else {
       /* Timeout occurred - implement SW reset*/
       while(1);
     }
  }
  
  if (PDU_sol.f_NavSOL == 1) {
    if(xSemaphoreTake(xUBXMutex, timeout) == pdTRUE) {
      taskENTER_CRITICAL();
      if (CAN1_MessageTransmit(0x478, 8, &PDU_sol.NavSOL_PartA[0], 1, 0, 0) == true) { HB_LED_Toggle(); }
      if (CAN1_MessageTransmit(0x479, 8, &PDU_sol.NavSOL_PartB[0], 1, 0, 0) == true) { HB_LED_Toggle(); }
      if (CAN1_MessageTransmit(0x480, 8, &PDU_sol.NavSOL_PartC[0], 1, 0, 0) == true) { HB_LED_Toggle(); }
      WDT_Clear();
      taskEXIT_CRITICAL();
  
      PDU_sol.f_NavSOL = 0;
  
      xSemaphoreGive(xUBXMutex);
    }
    else {
      /* Timeout occurred - implement SW reset*/
      while(1);   
    }
  }
  
  vTaskDelay(pdMS_TO_TICKS(250));
}

void vTaskSendUBXheartbeat(void)
{
  uint8_t GNSS_Heartbeat[8] = {0xA5, 0xA5, 0xDE, 0xAD, 0xBE, 0xEF, 0xA5, 0xA5};
  
  CAN1_MessageTransmit(0x481, 8, &GNSS_Heartbeat[0], 1, 0, 0);
  WDT_Clear();
  vTaskDelay(pdMS_TO_TICKS(2000));
  
}


/* Determine if a beginning of frame sequence has been found */
uint8_t UBXframeFind(uint8_t UBXByte, uint8_t* FrameType)
{
  static uint8_t FrameStartPosition = 0;
  uint8_t sUBXframeFound = 0;
  
  if ((UBXByte == 0xB5) && (FrameStartPosition == 0)) {
      FrameStartPosition = 1;
      sUBXframeFound = 1; /* UBX Frame Pending */
  }
  
  else if ((UBXByte == 0x62) && (FrameStartPosition == 1)) {
      FrameStartPosition = 2;
      sUBXframeFound = 2; /* UBX Frame Pending */
  }
  
  else if ((UBXByte == 0x01) && (FrameStartPosition == 2)) {
      FrameStartPosition = 3; 
      sUBXframeFound = 3; /* UBX Frame Pending */
  }
  
  else if (FrameStartPosition == 3) {
      FrameStartPosition = 0;
      *FrameType = UBXByte;
      sUBXframeFound = 4; /* UBX Frame Found.  */
  }
  
  else {
    FrameStartPosition = 0;
    sUBXframeFound = 5; /* Not a valid Frame header */
  }
  
  return sUBXframeFound;
}

/* Takes UBX byte and puts it in the appropriate UBX Buffer */
uint8_t UBXframeProcess(uint8_t UBXByte, uint8_t FrameType)
{
  static uint8_t UBX_BufPosCur = 0;
  uint8_t UBX_BufPosMax;
  uint8_t status;
  
  if(FrameType == UBX_FRAME_SOL) {
    UBX_BufPosMax = 56;   
  }
    
  else if(FrameType == UBX_FRAME_PVT) {
    UBX_BufPosMax = 96;
  }
    
  else if(FrameType == UBX_FRAME_TIMEGPS) {
    UBX_BufPosMax = 20;
  }
    
  else if(FrameType == UBX_FRAME_COV) {
    UBX_BufPosMax = 68;
  }
    
  else {
    UBX_BufPosCur = 0;
    UBX_BufPosMax = 100;
    status = 1; /* An invalid FrameType was received */ 
  }
  
  UBX_BUF[UBX_BufPosCur] = UBXByte;
  UBX_BufPosCur++;
  status = 2; /* Frame is being written */
  
  if (UBX_BufPosCur == UBX_BufPosMax) {
    UBX_BufPosCur = 0;
    status = 3; /* Frame written */
  }

  return status;  
}

/* Parse out data of interest from the appropriate UBX Buffer and place it into ready to transmit PDUs. Set Ready to Tx Flag. */
void PackPDU(uint8_t FrameType)
{
  switch (FrameType) {
    case UBX_FRAME_SOL:
      
      /* Signals: ecefX and ecefY */  
      PDU_sol.NavSOL_PartA[0] = UBX_BUF[18 - UBX_OFFSET];
      PDU_sol.NavSOL_PartA[1] = UBX_BUF[19 - UBX_OFFSET];
      PDU_sol.NavSOL_PartA[2] = UBX_BUF[20 - UBX_OFFSET];
      PDU_sol.NavSOL_PartA[3] = UBX_BUF[21 - UBX_OFFSET];
      PDU_sol.NavSOL_PartA[4] = UBX_BUF[22 - UBX_OFFSET];
      PDU_sol.NavSOL_PartA[5] = UBX_BUF[23 - UBX_OFFSET];
      PDU_sol.NavSOL_PartA[6] = UBX_BUF[24 - UBX_OFFSET];
      PDU_sol.NavSOL_PartA[7] = UBX_BUF[25 - UBX_OFFSET];
      
      /* Signals: ecefZ and ecefVX */
      PDU_sol.NavSOL_PartB[0] = UBX_BUF[26 - UBX_OFFSET];
      PDU_sol.NavSOL_PartB[1] = UBX_BUF[27 - UBX_OFFSET];
      PDU_sol.NavSOL_PartB[2] = UBX_BUF[28 - UBX_OFFSET];
      PDU_sol.NavSOL_PartB[3] = UBX_BUF[29 - UBX_OFFSET];
      PDU_sol.NavSOL_PartB[4] = UBX_BUF[34 - UBX_OFFSET];
      PDU_sol.NavSOL_PartB[5] = UBX_BUF[35 - UBX_OFFSET];
      PDU_sol.NavSOL_PartB[6] = UBX_BUF[36 - UBX_OFFSET];
      PDU_sol.NavSOL_PartB[7] = UBX_BUF[37 - UBX_OFFSET];
      
      /* Signals: eceVY and eceVZ */
      PDU_sol.NavSOL_PartC[0] = UBX_BUF[38 - UBX_OFFSET];
      PDU_sol.NavSOL_PartC[1] = UBX_BUF[39 - UBX_OFFSET];
      PDU_sol.NavSOL_PartC[2] = UBX_BUF[40 - UBX_OFFSET];
      PDU_sol.NavSOL_PartC[3] = UBX_BUF[41 - UBX_OFFSET];
      PDU_sol.NavSOL_PartC[4] = UBX_BUF[42 - UBX_OFFSET];
      PDU_sol.NavSOL_PartC[5] = UBX_BUF[43 - UBX_OFFSET];
      PDU_sol.NavSOL_PartC[6] = UBX_BUF[44 - UBX_OFFSET];
      PDU_sol.NavSOL_PartC[7] = UBX_BUF[45 - UBX_OFFSET];
      
      PDU_sol.f_NavSOL = 1;
      break;
        
    case UBX_FRAME_PVT:
        
      /* Signals: YY, MM, DD, HR, MIN, SEC, leapSec */
      PDU_pvt.NavPVT_Time[0] = UBX_BUF[10 - UBX_OFFSET];
      PDU_pvt.NavPVT_Time[1] = UBX_BUF[11 - UBX_OFFSET];
      PDU_pvt.NavPVT_Time[2] = UBX_BUF[12 - UBX_OFFSET];
      PDU_pvt.NavPVT_Time[3] = UBX_BUF[13 - UBX_OFFSET];
      PDU_pvt.NavPVT_Time[4] = UBX_BUF[14 - UBX_OFFSET];
      PDU_pvt.NavPVT_Time[5] = UBX_BUF[15 - UBX_OFFSET];
      PDU_pvt.NavPVT_Time[6] = UBX_BUF[16 - UBX_OFFSET];
      /* NavPVT_Time[7] = UBX_TIMEGPS[16 - UBX_OFFSET]; Signal is filled in from another Frame case '30' */
        
      /* Signals: FixType, numSV, VNEDD, PDOP */
      PDU_pvt.NavPVT_Misc[0] = UBX_BUF[26 - UBX_OFFSET];
      PDU_pvt.NavPVT_Misc[1] = UBX_BUF[29 - UBX_OFFSET];
      PDU_pvt.NavPVT_Misc[2] = UBX_BUF[62 - UBX_OFFSET];
      PDU_pvt.NavPVT_Misc[3] = UBX_BUF[63 - UBX_OFFSET];
      PDU_pvt.NavPVT_Misc[4] = UBX_BUF[64 - UBX_OFFSET];
      PDU_pvt.NavPVT_Misc[5] = UBX_BUF[65 - UBX_OFFSET];
      PDU_pvt.NavPVT_Misc[6] = UBX_BUF[82 - UBX_OFFSET];
      PDU_pvt.NavPVT_Misc[7] = UBX_BUF[83 - UBX_OFFSET];
        
      /* Signals: Lon, Lat */
      PDU_pvt.NavPVT_LatLon[0] = UBX_BUF[30 - UBX_OFFSET];
      PDU_pvt.NavPVT_LatLon[1] = UBX_BUF[31 - UBX_OFFSET];
      PDU_pvt.NavPVT_LatLon[2] = UBX_BUF[32 - UBX_OFFSET];
      PDU_pvt.NavPVT_LatLon[3] = UBX_BUF[33 - UBX_OFFSET];
      PDU_pvt.NavPVT_LatLon[4] = UBX_BUF[34 - UBX_OFFSET];
      PDU_pvt.NavPVT_LatLon[5] = UBX_BUF[35 - UBX_OFFSET];
      PDU_pvt.NavPVT_LatLon[6] = UBX_BUF[36 - UBX_OFFSET];
      PDU_pvt.NavPVT_LatLon[7] = UBX_BUF[37 - UBX_OFFSET];
        
      /* Signals: height, heightMSL */
      PDU_pvt.NavPVT_Height[0] = UBX_BUF[38 - UBX_OFFSET];
      PDU_pvt.NavPVT_Height[1] = UBX_BUF[39 - UBX_OFFSET];
      PDU_pvt.NavPVT_Height[2] = UBX_BUF[40 - UBX_OFFSET];
      PDU_pvt.NavPVT_Height[3] = UBX_BUF[41 - UBX_OFFSET];
      PDU_pvt.NavPVT_Height[4] = UBX_BUF[42 - UBX_OFFSET];
      PDU_pvt.NavPVT_Height[5] = UBX_BUF[43 - UBX_OFFSET];
      PDU_pvt.NavPVT_Height[6] = UBX_BUF[44 - UBX_OFFSET];
      PDU_pvt.NavPVT_Height[7] = UBX_BUF[45 - UBX_OFFSET];
        
      /* Signals: VNEDN, VNEDE */
      PDU_pvt.NavPVT_VNED[0] = UBX_BUF[54 - UBX_OFFSET];
      PDU_pvt.NavPVT_VNED[1] = UBX_BUF[55 - UBX_OFFSET];
      PDU_pvt.NavPVT_VNED[2] = UBX_BUF[56 - UBX_OFFSET];
      PDU_pvt.NavPVT_VNED[3] = UBX_BUF[57 - UBX_OFFSET];
      PDU_pvt.NavPVT_VNED[4] = UBX_BUF[58 - UBX_OFFSET];
      PDU_pvt.NavPVT_VNED[5] = UBX_BUF[59 - UBX_OFFSET];
      PDU_pvt.NavPVT_VNED[6] = UBX_BUF[60 - UBX_OFFSET];
      PDU_pvt.NavPVT_VNED[7] = UBX_BUF[61 - UBX_OFFSET];
      
      PDU_pvt.f_NavPVT = 1;
      break;
         
    case UBX_FRAME_TIMEGPS:
      /* Signals: leapSec  */
      PDU_pvt.NavPVT_Time[7] = UBX_BUF[16 - UBX_OFFSET];
      break;
          
    case UBX_FRAME_COV:
        
      /* Signals: posCOVNN, posCOVNE */
      PDU_cov.NavCOV_PartA[0] = UBX_BUF[22 - UBX_OFFSET];
      PDU_cov.NavCOV_PartA[1] = UBX_BUF[23 - UBX_OFFSET];
      PDU_cov.NavCOV_PartA[2] = UBX_BUF[24 - UBX_OFFSET];
      PDU_cov.NavCOV_PartA[3] = UBX_BUF[25 - UBX_OFFSET];
      PDU_cov.NavCOV_PartA[4] = UBX_BUF[26 - UBX_OFFSET];
      PDU_cov.NavCOV_PartA[5] = UBX_BUF[27 - UBX_OFFSET];
      PDU_cov.NavCOV_PartA[6] = UBX_BUF[28 - UBX_OFFSET];
      PDU_cov.NavCOV_PartA[7] = UBX_BUF[29 - UBX_OFFSET];
      
      /* Signals: posCOVND, velCOVNN */
      PDU_cov.NavCOV_PartB[0] = UBX_BUF[30 - UBX_OFFSET];
      PDU_cov.NavCOV_PartB[1] = UBX_BUF[31 - UBX_OFFSET];
      PDU_cov.NavCOV_PartB[2] = UBX_BUF[32 - UBX_OFFSET];
      PDU_cov.NavCOV_PartB[3] = UBX_BUF[33 - UBX_OFFSET];
      PDU_cov.NavCOV_PartB[4] = UBX_BUF[46 - UBX_OFFSET];
      PDU_cov.NavCOV_PartB[5] = UBX_BUF[47 - UBX_OFFSET];
      PDU_cov.NavCOV_PartB[6] = UBX_BUF[48 - UBX_OFFSET];
      PDU_cov.NavCOV_PartB[7] = UBX_BUF[49 - UBX_OFFSET];
      
      /* Signals: velCOVNE, velCOVND */
      PDU_cov.NavCOV_PartC[0] = UBX_BUF[50 - UBX_OFFSET];
      PDU_cov.NavCOV_PartC[1] = UBX_BUF[51 - UBX_OFFSET];
      PDU_cov.NavCOV_PartC[2] = UBX_BUF[52 - UBX_OFFSET];
      PDU_cov.NavCOV_PartC[3] = UBX_BUF[53 - UBX_OFFSET];
      PDU_cov.NavCOV_PartC[4] = UBX_BUF[54 - UBX_OFFSET];
      PDU_cov.NavCOV_PartC[5] = UBX_BUF[55 - UBX_OFFSET];
      PDU_cov.NavCOV_PartC[6] = UBX_BUF[56 - UBX_OFFSET];
      PDU_cov.NavCOV_PartC[7] = UBX_BUF[57 - UBX_OFFSET];
      
      PDU_cov.f_NavCOV = 1;
      break;
          
    default:
      break;
  }
  
  return;
}

void vTaskGetUART1bytes_Init(void)
{
  xTaskGetUART1bytes = NULL;
  DMAC_ChannelCallbackRegister(DMAC_CHANNEL_1, UARTDmaChannelHandler, 0);
  DMAC_ChannelTransfer(DMAC_CHANNEL_1, (const void *)&U1RXREG, 1, &readByte, sizeof(readByte), 1); /* DMA is set to run continuously */
}

void vTaskProcessUBXmessage_Init(void)
{
  xTaskProcessUBXmessage = NULL;
}

void vTaskSendUBXmessage_Init(void)
{
  xTaskSendUBXmessage = NULL;
}

void vTaskSendUBXheartbeat_Init(void)
{
  xTaskSendUBXheartbeat = NULL;
}

