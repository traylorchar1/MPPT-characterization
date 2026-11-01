/*******************************************************************************
CAN.c
Copyright (c) 2015, Samuel Lenius
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of Dilithium Power Systems LLC nor the
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
InitCAN, sendECAN, recvECAN, clearRxFlags, and InitDMACAN 
code was taken from Microchip Application Note 01249A
*******************************************************************************/

#include "can.h"
#include "fram.h"
#include "leds.h"
#include "main.h"
#include "oscillator.h"
#include "packet.h"
#include "pwm.h"
#include "queue.h"
#include "timer.h"
#include "tracker.h"
#include "photon_service.h"

#include "state.pb.h"
#include "params.pb.h"

#include <string.h>

/******************************************************************************/
/******************************************************************************/

#define ECAN1_MSG_BUF_LENGTH 4

#define COMMAND_ACK 0x05
#define COMMAND_RESET 0xFE
#define COMMAND_PING 0x07

#define USER_CODE_PING 0x02

/******************************************************************************/
/******************************************************************************/

extern volatile StateMessage state;
extern volatile ParamsMessage params;

/******************************************************************************/
/******************************************************************************/

uint16 ecan1msgBuf[ECAN1_MSG_BUF_LENGTH][8]__attribute__(
    (space(dma),aligned(ECAN1_MSG_BUF_LENGTH*16)));

/******************************************************************************/
/******************************************************************************/

void sendCANMsg(int header, uint8 *data, int length){
  canMsg txmsg;
  int i;
  int max_length = 8;

  if(length > max_length) return; 
  
  txmsg.buffer = (uint8)0;
  txmsg.frame_type = CAN_FRAME_STD;
  txmsg.message_type = CAN_MSG_DATA;
  txmsg.id = header;
  for(i = 0; i < length; i++){
    txmsg.data[i] = data[i];
  }
  txmsg.data_length = length;
  
  enqueueCANMessage(&txmsg);
}

/******************************************************************************/
/******************************************************************************/

void CanMeasurementPacketHandler(canMsg *msg){
    // check if it's an RTR, if so, reply with measurement packet
  //if(msg->message_type == CAN_MSG_RTR){
    //SendMeasurementPacket();
  //}
}

/******************************************************************************/
/******************************************************************************/

void CanParameterPacketHandler(canMsg *msg){
  // parameter packet
  // if the disable tracker bit is set, disable the tracker
  
  if((msg->data[0] & 0x01) == 0){
    state.control = ControlEnum_CAN_DISABLE;
  }else{
    if(state.control == ControlEnum_CAN_DISABLE){
      state.control = ControlEnum_INIT;
    }
  }
  
  // if the disable LEDs bit is set, disable the LEDs
  if((msg->data[0] & 0x02) == 0x02){
    enableLEDs(OFF);
  }else{
    enableLEDs(ON);
  }
}

/******************************************************************************/
/******************************************************************************/

void CanSendAck(){
  int16 message_id = 0xA0 + params.comm.can_base_addr;
  uint8 pkt_data[2] = {COMMAND_ACK, USER_CODE_PING};
  sendCANMsg(message_id, pkt_data, 2);
}

/******************************************************************************/
/******************************************************************************/

void MaybeCanReset(canMsg *msg){
  uint8 cmd;

  cmd = msg->data[0];

  switch(cmd){

    case COMMAND_RESET:
      // Check if magic bytes are correct
      if( (msg->data[4] == 45) && 
          (msg->data[5] == 78) && 
          (msg->data[6] == 69) && 
          (msg->data[7] == 21)) {
        disablePWM();
        SetPrechargeRelayState(OFF);
        SetMainRelayState(OFF);
        LED_R_OFF();
        LED_G_OFF();
        CanSendAck();
        delayMilliseconds(20);
        reset();
      }
      break;

    case COMMAND_PING:
      CanSendAck();
      break;

    default:
      break;
  }

}

/******************************************************************************/
/******************************************************************************/

void MaybeCanSetDac(canMsg *msg){
  uint16 cmd;
  uint8 c;

  // Check if magic bytes are correct
  if( (msg->data[4] == 45) && 
      (msg->data[5] == 78) && 
      (msg->data[6] == 69) && 
      (msg->data[7] == 21)) {
    CanSendAck();
    state.control = ControlEnum_TEST_MODE;
    for(c=0; c<2; c++){
      cmd = msg->data[c*2] + 0x100*msg->data[c*2+1];
      state.chan[c].dac = LimitDac(cmd);
    }
  }
}

/******************************************************************************/
/******************************************************************************/

void MaybeCanSetBurstMode(canMsg *msg){

  // Check if magic bytes are correct
  if( (msg->data[4] == 45) && 
      (msg->data[5] == 78) && 
      (msg->data[6] == 69) && 
      (msg->data[7] == 21)) {
    CanSendAck();
    state.n_burst_pkts = msg->data[0] + 0x100*msg->data[1];
    state.burst_pkts_ch = msg->data[2];
    state.burst_pkts_ch &= 0x01;
  }
}

/******************************************************************************/
/******************************************************************************/

static canMsg canRxMessage;
static canMsgBuffer send;

void CANRecvEventHandler(void){
  // pull message out of buffer and put into a sensible structure
  recvECAN(&canRxMessage);

  // compute message type
  int16 message_type = canRxMessage.id - params.comm.can_base_addr;

  // using switch table, select the proper message handler
  switch(message_type){
    case 0x00: break;
    case 0x10: CanParameterPacketHandler(&canRxMessage); break;
    case 0x11: CanParameterPacketHandler(&canRxMessage); break;
    case 0x20: break;
    case 0x30: break;
    case 0x50: break;
    case 0x60: break;
    case 0x70: break;
    case 0x80: break;
    case 0x90: break;
    case 0xA0: MaybeCanReset(&canRxMessage); break;
    case 0xB0: MaybeCanSetDac(&canRxMessage); break;
    case 0xC0: MaybeCanSetBurstMode(&canRxMessage); break;
    case 0xD0: PhotonReceive(&canRxMessage); break;
    case 0xE0: break;
    case 0xF0: break;
    default:   break;
  }
  return;   
} 

/******************************************************************************/
/******************************************************************************/

// Interrupt Service Routine 1
// No fast context save, and no variables stacked
void _ISR _C1Interrupt(void) {
  // check to see if the interrupt is caused by receive       
  if(C1INTFbits.RBIF) {
    // check to see if buffer 1 is full
    if(C1RXFUL1bits.RXFUL1) {     
      // set the buffer full flag and the buffer received flag
      canRxMessage.buffer_status=CAN_BUF_FULL;
      canRxMessage.buffer=1;
      CANRecvEventHandler();  
    }   
    // check to see if buffer 2 is full
    else if(C1RXFUL1bits.RXFUL2) {
      // set the buffer full flag and the buffer received flag
      canRxMessage.buffer_status=CAN_BUF_FULL;
      canRxMessage.buffer=2;
      CANRecvEventHandler();      
    }
    // check to see if buffer 3 is full
    else if(C1RXFUL1bits.RXFUL3) {
      // set the buffer full flag and the buffer received flag
      canRxMessage.buffer_status=CAN_BUF_FULL;
      canRxMessage.buffer=3;
      CANRecvEventHandler();        
    }
    // clear flag
    C1INTFbits.RBIF = 0;
  }
  else if(C1INTFbits.TBIF) {
    // clear flag/
    C1INTFbits.TBIF = 0;
    denqueueCANMessage();     
  }
  
  // clear interrupt flag
  IFS2bits.C1IF=0;
  C1INTFbits.RBOVIF = 0;
  return; 
}

/******************************************************************************/
/******************************************************************************/

uint8 IsCanTxBufferFull(void){
  return send.Status.BufferFull;
}

/******************************************************************************/
/******************************************************************************/

void enqueueCANMessage(canMsg *msg){
  // check if the buffer is full, if so, return  
  if(send.Status.BufferFull == 1) return;
  
  // put the message at the end of the buffer then increment pointer
  send.Buf[send.Back++] = *msg;

  // if the pointer is past the end, wrap it around
  if(send.Back > send.End)  send.Back = 0;
  
  // if there is no message currently being processed, pop one from the queue
  if(send.Status.MessagePending == 0){
    denqueueCANMessage();
  }

  return;
}

/******************************************************************************/
/******************************************************************************/

void denqueueCANMessage(void){
  // check to see if queue is empty
  if(send.Back == send.Front){
    send.Status.MessagePending = 0;
    return;
  }

  // if theres something in the queue, set the status bit
  if(send.Status.MessagePending == 0){
    send.Status.MessagePending = 1;
  }
  
  // run the handler and increment pointer
  sendECAN(&send.Buf[send.Front++]);

  // if the pointer is past the end, wrap it back to the start
  if(send.Front > send.End) send.Front = 0;
  
  return;
}

/*******************************************************************************
*                                                                             
*    Function:      sendECAN
*    Description:       sends the message on a CAN bus  
*                                                                             
*    Arguments:     *msg: a pointer to the message structure
*           containing all the information about the message 
*  Author:            Jatinder Gharoo                                                      
*                                                                  
*                                                                              
*******************************************************************************/
void sendECAN(canMsg *msg)
{
  unsigned long word0=0;
  unsigned long word1=0;
  unsigned long word2=0;
  
  /*
  msg Format: 
  Word0 : 0bUUUx xxxx xxxx xxxx
           |____________|||
          SID10:0   SRR IDE(bit 0)     
  Word1 : 0bUUUU xxxx xxxx xxxx
             |____________|
            EID17:6
  Word2 : 0bxxxx xxx0 UUU0 xxxx
        |_____||       |__|
        EID5:0 RTR      DLC
  
  Remote Transmission Request Bit for standard frames 
  SRR-> "0"  Normal Message 
      "1"  Message will request remote transmission
  Substitute Remote Request Bit for extended frames 
  SRR-> should always be set to "1" as per CAN specification
  
  Extended  Identifier Bit      
  IDE->   "0"  Message will transmit standard identifier
        "1"  Message will transmit extended identifier
  
  Remote Transmission Request Bit for extended frames 
  RTR->   "0"  Message transmitted is a normal message
      "1"  Message transmitted is a remote message
  Don't care for standard frames 
  */
  
    
  /* check to see if the message has an extended ID */
  if(msg->frame_type==CAN_FRAME_EXT)
  {
    /* get the extended message id EID28..18*/    
    word0=(msg->id & 0x1FFC0000) >> 16;     
    /* set the SRR and IDE bit */
    word0=word0+0x0003;
    /* the the value of EID17..6 */
    word1=(msg->id & 0x0003FFC0) >> 6;
    /* get the value of EID5..0 for word 2 */
    word2=(msg->id & 0x0000003F) << 10;     
  } 
  else
  {
    /* get the SID */
    word0=((msg->id & 0x000007FF) << 2);  
  }
  /* check to see if the message is an RTR message */
  if(msg->message_type==CAN_MSG_RTR)
  {   
    if(msg->frame_type==CAN_FRAME_EXT)
      word2=word2 | 0x0200;
    else
      word0=word0 | 0x0002; 
                
    ecan1msgBuf[msg->buffer][0]=word0;
    ecan1msgBuf[msg->buffer][1]=word1;
    ecan1msgBuf[msg->buffer][2]=word2;
  }
  else
  {
    word2=word2+(msg->data_length & 0x0F);
    ecan1msgBuf[msg->buffer][0]=word0;
    ecan1msgBuf[msg->buffer][1]=word1;
    ecan1msgBuf[msg->buffer][2]=word2;
    /* fill the data */
    ecan1msgBuf[msg->buffer][3]=
            ((msg->data[1] << 8) + msg->data[0]);
    ecan1msgBuf[msg->buffer][4]=
            ((msg->data[3] << 8) + msg->data[2]);
    ecan1msgBuf[msg->buffer][5]=
            ((msg->data[5] << 8) + msg->data[4]);
    ecan1msgBuf[msg->buffer][6]=
            ((msg->data[7] << 8) + msg->data[6]);
  }
  /* set the message for transmission */
  C1TR01CONbits.TXREQ0=1;
  enableCANTXInterrupt();
}

/*******************************************************************************
*                                                                             
*    Function:      rxECAN
*    Description:       moves the message from the DMA memory to RAM
*                                                                             
*    Arguments:     *msg: a pointer to the message structure in RAM 
*           that will store the message. 
*  Author:            Jatinder Gharoo                                                      
*                                                                  
*                                                                              
*******************************************************************************/
void recvECAN(canMsg *msg) {
  unsigned int ide=0;
  unsigned int rtr=0;
  unsigned long id=0;
  
  /*
  Standard Message Format: 
  Word0 : 0bUUUx xxxx xxxx xxxx
           |____________|||
          SID10:0   SRR IDE(bit 0)     
  Word1 : 0bUUUU xxxx xxxx xxxx
             |____________|
            EID17:6
  Word2 : 0bxxxx xxx0 UUU0 xxxx
        |_____||       |__|
        EID5:0 RTR      DLC
  word3-word6: data bytes
  word7: filter hit code bits
  
  Remote Transmission Request Bit for standard frames 
  SRR-> "0"  Normal Message 
      "1"  Message will request remote transmission
  Substitute Remote Request Bit for extended frames 
  SRR-> should always be set to "1" as per CAN specification
  
  Extended  Identifier Bit      
  IDE->   "0"  Message will transmit standard identifier
        "1"  Message will transmit extended identifier
  
  Remote Transmission Request Bit for extended frames 
  RTR->   "0"  Message transmitted is a normal message
      "1"  Message transmitted is a remote message
  Don't care for standard frames 
  */
  
    
  // read word 0 to see the message type
  ide=ecan1msgBuf[msg->buffer][0] & 0x0001;     
  
  // check to see what type of message it is
  if(ide == 0) {
    // message is standard identifier
    msg->id=(ecan1msgBuf[msg->buffer][0] & 0x1FFC) >> 2;    
    msg->frame_type=CAN_FRAME_STD;
    rtr=ecan1msgBuf[msg->buffer][0] & 0x0002;
  }
  else {
    // message is extended identifier
    id = ecan1msgBuf[msg->buffer][0] & 0x1FFC;    
    msg->id = id << 16;

    id = ecan1msgBuf[msg->buffer][1] & 0x0FFF;
    msg->id = msg->id+(id << 6);

    id = (ecan1msgBuf[msg->buffer][2] & 0xFC00) >> 10;
    msg->id = msg->id+id;

    msg->frame_type = CAN_FRAME_EXT;

    rtr = ecan1msgBuf[msg->buffer][2] & 0x0200;
  }

  // check to see what type of message it is
  if(rtr != 0) {
    // RTR message
    msg->message_type=CAN_MSG_RTR;  
  }else {
    // normal message
    msg->message_type=CAN_MSG_DATA;
    msg->data[0]=(uint8)ecan1msgBuf[msg->buffer][3];
    msg->data[1]=(uint8)((ecan1msgBuf[msg->buffer][3] & 0xFF00) >> 8);
    msg->data[2]=(uint8)ecan1msgBuf[msg->buffer][4];
    msg->data[3]=(uint8)((ecan1msgBuf[msg->buffer][4] & 0xFF00) >> 8);
    msg->data[4]=(uint8)ecan1msgBuf[msg->buffer][5];
    msg->data[5]=(uint8)((ecan1msgBuf[msg->buffer][5] & 0xFF00) >> 8);
    msg->data[6]=(uint8)ecan1msgBuf[msg->buffer][6];
    msg->data[7]=(uint8)((ecan1msgBuf[msg->buffer][6] & 0xFF00) >> 8);
    msg->data_length=(uint8)(ecan1msgBuf[msg->buffer][2] & 0x000F);
  }

  // clear the flags
  clearRxFlags(msg->buffer);
}

/*******************************************************************************
*******************************************************************************/

void clearRxFlags(uint8 buffer_number)
{
  if((C1RXFUL1bits.RXFUL1) && (buffer_number==1))
    /* clear flag */
    C1RXFUL1bits.RXFUL1=0;    
  /* check to see if buffer 2 is full */
  else if((C1RXFUL1bits.RXFUL2) && (buffer_number==2))
    /* clear flag */
    C1RXFUL1bits.RXFUL2=0;        
  /* check to see if buffer 3 is full */
  else if((C1RXFUL1bits.RXFUL3) && (buffer_number==3))
    /* clear flag */
    C1RXFUL1bits.RXFUL3=0;        
  else{
        nop();
    }
}


/*******************************************************************************
*******************************************************************************/
void InitCAN(){
  
  unsigned int loops;
  
  /* put the module in configuration mode */
  C1CTRL1bits.REQOP=4;
  while(C1CTRL1bits.OPMODE != 4);
      
  /* FCAN is selected to be FCY
    FCAN = FCY = 40MHz */
  //C1CTRL1bits.CANCKS = 0x1;

  /*
  Bit Time = (Sync Segment + 
              Propagation Delay + 
              Phase Segment 1 + 
              Phase Segment 2) = 20*TQ
  Phase Segment 1 = 8TQ
  Phase Segment 2 = 6Tq
  Propagation Delay = 5Tq
  Sync Segment = 1TQ
  CiCFG1<BRP> =(FCAN /(2 ×N×FBAUD))– 1
  BIT RATE OF 1Mbps
  */  
  C1CFG1bits.BRP = ((FCAN/(2 * NTQ * params.comm.can_bitrate))-1);
  /* Synchronization Jump Width set to 4 TQ */
  C1CFG1bits.SJW = 0x3;
  /* Phase Segment 1 time is 8 TQ */
  C1CFG2bits.SEG1PH=0x7;
  /* Phase Segment 2 time is set to be programmable */
  C1CFG2bits.SEG2PHTS = 0x1;
  /* Phase Segment 2 time is 6 TQ */
  C1CFG2bits.SEG2PH = 0x5;
  /* Propagation Segment time is 5 TQ */
  C1CFG2bits.PRSEG = 0x4;
  /* Bus line is sampled three times at the sample point */
  C1CFG2bits.SAM = 0x1;
  
  /* 4 CAN Messages to be buffered in DMA RAM */  
  C1FCTRLbits.DMABS = 0x00;
  
  /* Filter configuration */
  /* enable window to access the filter configuration registers */
  C1CTRL1bits.WIN = 0x01;
    
  // configure acceptance buffer 0

  // select acceptance mask 0 filter 0 buffer 1
  C1FMSKSEL1bits.F0MSK = 0x00;
  C1RXM0SIDbits.SID = 0x07F0;
  C1RXF0SIDbits.SID = 0x0600;
  // set filter to check for standard ID and accept standard id only
  C1RXM0SIDbits.MIDE = 0x1;
  C1RXF0SIDbits.EXIDE = 0x0;
  // acceptance filter to use buffer 1 for incoming messages
  C1BUFPNT1bits.F0BP = 0x01;
  // enable filter 0
  C1FEN1bits.FLTEN0 = 1;
  
  // configure acceptance buffer 1

  // select acceptance mask 1 filter 1 buffer 2
  C1FMSKSEL1bits.F0MSK = 0x01;
  C1RXM1SIDbits.SID = 0x0700;
  C1RXF1SIDbits.SID = 0x0600;
  // set filter to check for standard ID and accept standard id only
  C1RXM1SIDbits.MIDE = 0x1;
  C1RXF1SIDbits.EXIDE = 0x0;
  // acceptance filter to use buffer 1 for incoming messages
  C1BUFPNT1bits.F1BP = 0x02;
  // enable filter 0
  C1FEN1bits.FLTEN1 = 1;
  
           
  /* clear window bit to access ECAN control registers */
  C1CTRL1bits.WIN=0;
    
  /* put the module in normal mode */
  C1CTRL1bits.REQOP=0;
    
  // spin until opmode goes low
  loops = 0xffff; 
  while(loops > 0){
    if(C1CTRL1bits.OPMODE == 0){
      break;
    }
    loops--;
  }

  /* clear the buffer and overflow flags */
  C1RXFUL1=C1RXFUL2=C1RXOVF1 = 0x0000;
  C1RXOVF2 = 0x0000;
  /* ECAN1, Buffer 0 is a Transmit Buffer */
  C1TR01CONbits.TXEN0 = 1;      
  /* ECAN1, Buffer 1 is a Receive Buffer */
  C1TR01CONbits.TXEN1 = 0;  
  /* ECAN1, Buffer 2 is a Receive Buffer */
  C1TR23CONbits.TXEN2 = 0;  
  /* ECAN1, Buffer 3 is a Receive Buffer */
  C1TR23CONbits.TXEN3 = 0;  
  /* Message Buffer 0 Priority Level */
  C1TR01CONbits.TX0PRI = 0x03;    
    
  /* configure the device to interrupt on the receive buffer full flag */
  /* clear the buffer full flags */
  C1RXFUL1=0;
  C1INTFbits.RBIF=0;
  
  InitDMACAN();
  
  send.Front = 0;
  send.Back = 0;
  send.End = 15;
  send.Space = 15;
  send.Status.BufferFull = 0;
  send.Status.BufferEmpty = 0;
  send.Status.MessagePending = 0;
  
  enableCANMainInterrupt();
  
  enableCANRXInterrupt(); 
  
}

/*******************************************************************************
*                                                                             
*    Function:      InitDMACAN
*    Description:       Initialises the DMA to be used with ECAN module                                                        
*                       Channel 0 of the DMA is configured to Tx ECAN messages
*             of ECAN module 1. 
*           Channel 2 is uconfigured to Rx ECAN messages of module 1.                                                      
*    Arguments:     
*  Author:            Jatinder Gharoo                                                      
*                                                                  
*                                                                              
*******************************************************************************/
void InitDMACAN(void)
{
  
  /* Initialise the DMA channel 1 for ECAN Tx */
  /* clear the collission flags */
  DMACS0=0; 
  /* setup channel 1 for peripheral indirect addressing mode 
  normal operation, word operation and select as Tx to peripheral */
  DMA1CON=0x2020; 
  /* setup the address of the peripheral ECAN1 (C1TXD) */ 
  DMA1PAD=0x0642;
  /* Set the data block transfer size of 8 */
  DMA1CNT=7;
  /* automatic DMA Tx Initiation by DMA request */
  DMA1REQ=0x0046; 
  /* DPSRAM atart adddress offset value */ 
  DMA1STA=__builtin_dmaoffset(&ecan1msgBuf);  
  /* enable the channel */
  DMA1CONbits.CHEN=1;
  
  /* Initialise the DMA channel 2 for ECAN Rx */
  /* clear the collission flags */
  DMACS0=0;
  /* setup channel 2 for peripheral indirect addressing mode 
  normal operation, word operation and select as Rx to peripheral */
  DMA2CON=0x0020;
  /* setup the address of the peripheral ECAN1 (C1RXD) */ 
  DMA2PAD=0x0640; 
  /* Set the data block transfer size of 8 */
  DMA2CNT=7;
  /* automatic DMA Rx Initiation by DMA request */
  DMA2REQ=0x0022; 
  /* DPSRAM atart adddress offset value */ 
  DMA2STA=__builtin_dmaoffset(&ecan1msgBuf);  
  /* enable the channel */
  DMA2CONbits.CHEN=1;
}

