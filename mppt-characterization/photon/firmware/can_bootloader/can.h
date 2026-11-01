/*******************************************************************************
CAN.h
Copyright (c) 2015, Samuel Lenius
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of DILITHIUM POWER SYSTEMS LLC nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY DILITHIUM POWER SYSTEMS LLC ''AS IS'' AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL DILITHIUM POWER SYSTEMS LLC BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*******************************************************************************/

/*******************************************************************************
Most of this code was taken from Microchip Application Note 01249A
*******************************************************************************/



#ifndef CAN_H
#define CAN_H

#include "general.h"

#define enableCANMainInterrupt() {IEC2bits.C1IE = 1;}
#define disableCANMainInterrupt() {IEC2bits.C1IE = 0;}

#define enableCANTXInterrupt() {C1INTEbits.TBIE = 1;}
#define disableCANTXInterrupt() {C1INTEbits.TBIE = 0;}

#define enableCANRXInterrupt() {C1INTEbits.RBIE = 1;}
#define disableCANRXInterrupt() {C1INTEbits.RBIE = 0;}

#define CAN_MSG_DATA  (uint8)0x01
#define CAN_MSG_RTR    (uint8)0x02
#define CAN_FRAME_EXT  (uint8)0x03
#define CAN_FRAME_STD  (uint8)0x04
#define CAN_BUF_FULL  (uint8)0x05
#define CAN_BUF_EMPTY  (uint8)0x06

/* CAN Baud Rate Configuration     */
#define FCAN            OSC_FCY 
#define NTQ       20    // 20 Time Quanta in a Bit Time

typedef struct{
  /* keep track of the buffer status */
  unsigned char buffer_status;
  /* RTR message or data message */
  unsigned char message_type;
  /* frame type extended or standard */
  unsigned char frame_type;
  /* buffer being used to send and receive messages */
  unsigned char buffer;
  /* 29 bit id max of 0x1FFF FFFF 
  *  11 bit id max of 0x7FF */
  unsigned long id; 
  unsigned char data[8];  
  unsigned char data_length;
}canMsg;

typedef struct{
  canMsg Buf[16];
  uint16 Front;   
  uint16 Back;
  uint16 End;
  uint16 Space;
  struct tagCANStatus {
    unsigned BufferFull:1;
    unsigned BufferEmpty:1;
    unsigned MessagePending:1;
  } Status;
}canMsgBuffer;


void InitCan();
void CanPseudoInterrupt();
void sendECan(canMsg *msg);
void recvECan(canMsg *msg);
void clearRxFlags(unsigned char buffer_number);
void InitDMACan(void);
void CanRecvEventHandler(void);
void enqueueCanMessage(canMsg *msg);
void denqueueCanMessage(void);
void sendCanMsg(int header, unsigned char *data, int length);
void CanProgram(canMsg *msg);
void CanSendAck(void);

#endif
