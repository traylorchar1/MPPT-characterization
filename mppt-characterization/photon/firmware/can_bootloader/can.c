/*******************************************************************************
can.c
Copyright (c) 2015, Samuel Lenius
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of UMNSVP nor the
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
#include "debug.h"
#include "fram.h"
#include "leds.h"
#include "main.h"
#include "oscillator.h"
#include "prog_memory.h"
#include "timer.h"

#include "../photon_3/Photon_3_0/params.pb.h"

#ifdef DEBUG_EN
  #include <string.h>
  #include <stdio.h>
#endif

/******************************************************************************/
/******************************************************************************/

#define ECAN1_MSG_BUF_LENGTH 4

#define COMMAND_READ_LATCH        0x01
#define COMMAND_WRITE_LATCH       0x02
#define COMMAND_WRITE_BLOCK       0x03
#define COMMAND_ERASE_BLOCK       0x04
#define COMMAND_ACK               0x05
#define COMMAND_JUMP_TO_PROGRAM   0x06
#define COMMAND_PING              0x07
#define COMMAND_WRITE_FRAM        0x08
#define COMMAND_READ_FRAM         0x09
#define COMMAND_READ_HW_REV       0x0A

#define COMMAND_WRITE_FLASH       0x10
#define COMMAND_READ_FLASH        0x11
#define COMMAND_ERASE_FLASH       0x12

#define COMMAND_RESET             0xFE


#define BOOTLOADER_PING           0x01

#define BOOTLOADER_START_ADDR     0x7000

/******************************************************************************/
/******************************************************************************/

extern volatile ParamsMessage params;

/******************************************************************************/
/******************************************************************************/

extern void JumpToUserCode(void);

uint16 ecan1msgBuf[ECAN1_MSG_BUF_LENGTH][8]__attribute__(
    (space(dma),aligned(ECAN1_MSG_BUF_LENGTH*16)));

/******************************************************************************/
/******************************************************************************/

void sendCanMsg(int header, uint8 *data, int length){
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
  
  sendECan(&txmsg);  
}

/******************************************************************************/
/******************************************************************************/

void CanSendAck(){
  int16 message_id = 0xA0 + params.comm.can_base_addr;
  uint8 pkt_data[2] = {COMMAND_ACK, BOOTLOADER_PING};
  sendCanMsg(message_id, pkt_data, 2); 
}

/******************************************************************************/
/******************************************************************************/

void CanProgram(canMsg *msg){
  uint8 cmd, i;
  union u_t addr, in_data, out_data;

  uint8 pkt_data[8] = {0, 0, 0, 0, 0, 0, 0, 0};

  #ifdef DEBUG_EN
    char buf[0x100];
  #endif

  StopCountdownTimer();

  cmd = msg->data[0];

  addr.u8[0] = msg->data[1];
  addr.u8[1] = msg->data[2];
  addr.u8[2] = msg->data[3];
  addr.u8[3] = 0;

  in_data.u8[0] = msg->data[4];
  in_data.u8[1] = msg->data[5];
  in_data.u8[2] = msg->data[6];
  in_data.u8[3] = msg->data[7];


  switch(cmd){

    case COMMAND_READ_LATCH:
      out_data = ReadPmLatch(addr);
      #ifdef DEBUG_EN
        sprintf(buf, "rd_lat\t%02x\taddr\t%04x%04x\tdata\t%04x%04x\r\n",
          cmd, addr.u16[1], addr.u16[0], out_data.u16[1], out_data.u16[0]); 
        debug_puts(buf);
      #endif
      pkt_data[0] = cmd;
      
      pkt_data[1] = addr.u8[0];
      pkt_data[2] = addr.u8[1];
      pkt_data[3] = addr.u8[2];

      pkt_data[4] = out_data.u8[0];
      pkt_data[5] = out_data.u8[1];
      pkt_data[6] = out_data.u8[2];
      pkt_data[7] = out_data.u8[3];

      sendCanMsg(msg->id, pkt_data, 8); 
      break;

    case COMMAND_WRITE_LATCH:
      #ifdef DEBUG_EN
        sprintf(buf, "wr_lat\t%02x\taddr\t%04x%04x\tdata\t%04x%04x\r\n",
          cmd, addr.u16[1], addr.u16[0], in_data.u16[1], in_data.u16[0]); 
        debug_puts(buf);
      #endif
      // This prevents us from clobbering the initial jump instruction.
      if(addr.u32 == 0){
        in_data.u32 = 0x040000 + BOOTLOADER_START_ADDR;
      }
      // If the address is in the allowable range then program
      if(addr.u32 < BOOTLOADER_START_ADDR){
        WritePmLatch(addr, in_data);
        CanSendAck();
      }
      break;

    case COMMAND_WRITE_BLOCK:
      #ifdef DEBUG_EN
        sprintf(buf, "wr_blk\t%02x\taddr\t%04x%04x\tdata\t%04x%04x\r\n",
          cmd, addr.u16[1], addr.u16[0], in_data.u16[1], in_data.u16[0]); 
        debug_puts(buf);
      #endif
      WritePmBlock();
      CanSendAck();
      break;

    case COMMAND_ERASE_BLOCK:
      #ifdef DEBUG_EN
        sprintf(buf, "eblk\t%02x\taddr\t%04x%04x\tdata\t%04x%04x\r\n",
          cmd, addr.u16[1], addr.u16[0], in_data.u16[1], in_data.u16[0]); 
        debug_puts(buf);
      #endif
      // If the address is in the allowable range then program
      if(addr.u32 < BOOTLOADER_START_ADDR){
        ErasePmBlock(addr);
        CanSendAck();
      }
      break;
    
    case COMMAND_READ_FRAM:
      out_data.u32 = 0;
      // If the address is in the allowable range then program
      if(addr.u32 < FRAM_SIZE){
        for(i=0; i<4; i++){
          out_data.u8[i] = ReadWriteByteFram(addr.u16[0] + i, 0, FRAM_READ);
        }
        #ifdef DEBUG_EN
          sprintf(buf, "rd_fram\t%02x\taddr\t%04x%04x\tdata\t%04x%04x\r\n",
            cmd, addr.u16[1], addr.u16[0], out_data.u16[1], out_data.u16[0]); 
          debug_puts(buf);
        #endif
        pkt_data[0] = cmd;
        
        pkt_data[1] = addr.u8[0];
        pkt_data[2] = addr.u8[1];
        pkt_data[3] = addr.u8[2];
  
        pkt_data[4] = out_data.u8[0];
        pkt_data[5] = out_data.u8[1];
        pkt_data[6] = out_data.u8[2];
        pkt_data[7] = out_data.u8[3];
  
        sendCanMsg(msg->id, pkt_data, 8);
      } 
      break;

    case COMMAND_READ_HW_REV:
      out_data.u32 = ReadHwRevision();
      pkt_data[0] = cmd;
        
      pkt_data[1] = 0;
      pkt_data[2] = 0;
      pkt_data[3] = 0;
  
      pkt_data[4] = out_data.u8[0];
      pkt_data[5] = out_data.u8[1];
      pkt_data[6] = out_data.u8[2];
      pkt_data[7] = out_data.u8[3];
  
      sendCanMsg(msg->id, pkt_data, 8);
       
      break;

    case COMMAND_WRITE_FRAM:
      #ifdef DEBUG_EN
        sprintf(buf, "wr_fram\t%02x\taddr\t%04x%04x\tdata\t%04x%04x\r\n",
          cmd, addr.u16[1], addr.u16[0], in_data.u16[1], in_data.u16[0]); 
        debug_puts(buf);
      #endif
      for(i=0; i<4; i++){
        ReadWriteByteFram(addr.u16[0] + i, in_data.u8[i], FRAM_WRITE);
      }
      CanSendAck();
      break;

    case COMMAND_READ_FLASH:
      out_data.u32 = 0;
      // If the address is in the allowable range then program
      ReadBytesFlash(addr.u32, out_data.u8, 4);
      #ifdef DEBUG_EN
        sprintf(buf, "rd_flash\t%02x\taddr\t%04x%04x\tdata\t%04x%04x",
          cmd, addr.u16[1], addr.u16[0], out_data.u16[1], out_data.u16[0]); 
        debug_putl(buf);
      #endif
      pkt_data[0] = cmd;
        
      pkt_data[1] = addr.u8[0];
      pkt_data[2] = addr.u8[1];
      pkt_data[3] = addr.u8[2];
  
      pkt_data[4] = out_data.u8[0];
      pkt_data[5] = out_data.u8[1];
      pkt_data[6] = out_data.u8[2];
      pkt_data[7] = out_data.u8[3];
  
      sendCanMsg(msg->id, pkt_data, 8);
       
      break;

    case COMMAND_WRITE_FLASH:
      #ifdef DEBUG_EN
        sprintf(buf, "wr_flash\t%02x\taddr\t%04x%04x\tdata\t%04x%04x",
          cmd, addr.u16[1], addr.u16[0], in_data.u16[1], in_data.u16[0]); 
        debug_putl(buf);
      #endif
      WriteBytesFlash(addr.u32, in_data.u8, 4);
      CanSendAck();
      break;

    case COMMAND_ERASE_FLASH:
      #ifdef DEBUG_EN
        sprintf(buf, "erase flash\t%02x\taddr\t%04x%04x\tdata\t%04x%04x\r\n",
          cmd, addr.u16[1], addr.u16[0], in_data.u16[1], in_data.u16[0]); 
        debug_puts(buf);
      #endif
      EraseFlash();
      CanSendAck();
      break;

    case COMMAND_RESET:
      // Check if magic bytes are correct
      if( (msg->data[4] == 45) &&
          (msg->data[5] == 78) &&
          (msg->data[6] == 69) &&
          (msg->data[7] == 21)) {
        CanSendAck();
        debug_puts("RESET\r\n");
        delayMilliseconds(20);
        reset();
      }
      break;

    case COMMAND_PING:
      CanSendAck();
      debug_puts("PING\r\n");
      break;

    case COMMAND_JUMP_TO_PROGRAM:
      CanSendAck();
      debug_puts("JUMP_TO_PROGRAM\r\n");
      delayMilliseconds(20);
      JumpToUserCode();
      break;

    default:
      #ifdef DEBUG_EN
        sprintf(buf, "cmd\t%02x\taddr\t%04x%04x\tdata\t%04x%04x\r\n",
              cmd, addr.u16[1], addr.u16[0], in_data.u16[1], in_data.u16[0]); 
        debug_puts(buf);
      #endif
      break;
  }

}

/******************************************************************************/
/******************************************************************************/

static canMsg canRxMessage;
static canMsgBuffer send;

void CanRecvEventHandler(void){
  // pull message out of buffer and put into a sensible structure
 
  recvECan(&canRxMessage);

  // compute message type
  int16 message_type = canRxMessage.id - params.comm.can_base_addr;

  // using switch table, select the proper message handler
  switch(message_type){
    case 0x00: break;
    case 0x10: break;
    case 0x11: break;
    case 0x20: break;
    case 0x30: break;
    case 0x50: break;
    case 0x60: break;
    case 0x70: break;
    case 0x80: break;
    case 0x90: break;
    case 0xA0: CanProgram(&canRxMessage); break;
    case 0xB0: break;
    case 0xC0: break;
    case 0xD0: break;
    case 0xE0: break;
    case 0xF0: break;
    default:   break;
  }
} 

/******************************************************************************/
/******************************************************************************/

// Don't use interrupts in bootloader - this has to spin
void CanPseudoInterrupt(void) {

  // check to see if buffer 1 is full
  if(C1RXFUL1bits.RXFUL1) {     
    // set the buffer full flag and the buffer received flag
    canRxMessage.buffer_status=CAN_BUF_FULL;
    canRxMessage.buffer=1;
    CanRecvEventHandler();  
  }   
  // check to see if buffer 2 is full
  else if(C1RXFUL1bits.RXFUL2) {
    // set the buffer full flag and the buffer received flag
    canRxMessage.buffer_status=CAN_BUF_FULL;
    canRxMessage.buffer=2;
    CanRecvEventHandler();      
  }
  // check to see if buffer 3 is full
  else if(C1RXFUL1bits.RXFUL3) {
    // set the buffer full flag and the buffer received flag
    canRxMessage.buffer_status=CAN_BUF_FULL;
    canRxMessage.buffer=3;
    CanRecvEventHandler();        
  }
  
  // clear interrupt flag
  IFS2bits.C1IF=0;
  C1INTFbits.RBOVIF = 0;
  return; 
}

/******************************************************************************/
/******************************************************************************/

void enqueueCanMessage(canMsg *msg){
  uint16 i;
  // check if the buffer is full, if so, return  
  
  if(send.Status.BufferFull == 1){
    return;
  }
  
  // put the message at the end of the buffer then increment pointer
  send.Buf[send.Back++].id = msg->id;
  send.Buf[send.Back++].message_type = msg->message_type;
  send.Buf[send.Back++].frame_type = msg->frame_type;
  send.Buf[send.Back++].data_length = msg->data_length;
  for(i=0; i<msg->data_length; i++){
    send.Buf[send.Back++].data[i] = msg->data[i];
  }
  // if the pointer is past the end, wrap it around
  if(send.Back > send.End)  send.Back = 0;
  
  // if there is no message currently being processed, pop one from the queue
  if(send.Status.MessagePending == 0){
    denqueueCanMessage();
  }

  return;
}

/******************************************************************************/
/******************************************************************************/

void denqueueCanMessage(void){
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
  sendECan(&send.Buf[send.Front++]);

  // if the pointer is past the end, wrap it back to the start
  if(send.Front > send.End) send.Front = 0;
  
  return;
}

/*******************************************************************************
*                                                                             
*    Function:      sendECan
*    Description:       sends the message on a CAN bus  
*                                                                             
*    Arguments:     *msg: a pointer to the message structure
*           containing all the information about the message 
*  Author:            Jatinder Gharoo                                                      
*                                                                  
*                                                                              
*******************************************************************************/
void sendECan(canMsg *msg)
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
  //enableCanTXInterrupt();
}

/*******************************************************************************
*                                                                             
*    Function:      rxECan
*    Description:       moves the message from the DMA memory to RAM
*                                                                             
*    Arguments:     *msg: a pointer to the message structure in RAM 
*           that will store the message. 
*  Author:            Jatinder Gharoo                                                      
*                                                                  
*                                                                              
*******************************************************************************/
void recvECan(canMsg *msg) {
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
void InitCan(){
  
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
  CiCFG1<BRP> =(FCAN /(2 ×N×FBAUD)) 1
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
  for(loops=0; loops<0xffff; loops++){
    if(C1CTRL1bits.OPMODE == 0){
      break;
    }
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
  
  InitDMACan();
  
  send.Front = 0;
  send.Back = 0;
  send.End = 15;
  send.Space = 15;
  send.Status.BufferFull = 0;
  send.Status.BufferEmpty = 0;
  send.Status.MessagePending = 0;
 

  C1INTFbits.RBIF = 0;
  C1INTFbits.RBOVIF = 0;
  IFS2bits.C1IF = 0;

  IEC2bits.C1IE = 0; // Disable interrupts for ECAN1 peripheral
  C1INTEbits.TBIE = 0; // Disable TX buffer interrupt
  C1INTEbits.RBIE = 0; // Disable RX buffer interrupt
  C1INTEbits.ERRIE = 0;
  C1INTEbits.IVRIE = 0;
  C1INTEbits.RBOVIE = 0;

}

/*******************************************************************************
*                                                                             
*    Function:      InitDMACAN
*    Description:   Initialises the DMA to be used with ECAN module
*                   Channel 0 of the DMA is configured to Tx ECAN messages
*                   of ECAN module 1. 
*                   Channel 2 is uconfigured to Rx ECAN messages of module 1.
*   Arguments:     
*   Author:            Jatinder Gharoo                                   
*******************************************************************************/
void InitDMACan(void)
{
 
  // dma reference:
  // http://ww1.microchip.com/downloads/en/DeviceDoc/70182C.pdf
  // req - page 22-6
  // pas - page

  /* Initialise the DMA channel 1 for ECAN Tx */
  /* clear the collision flags */
  DMACS0 = 0; 
  /* setup channel 1 for peripheral indirect addressing mode 
  normal operation, word operation and select as Tx to peripheral */
  DMA1CON = 0x2020; 
  /* setup the address of the peripheral ECAN1 (C1TXD) */ 
  DMA1PAD = (volatile unsigned int) &C1TXD;
  /* Set the data block transfer size of 8 */
  DMA1CNT = 7;
  /* automatic DMA Tx Initiation by DMA request */
  DMA1REQ = 0x0046; 
  /* DPSRAM atart adddress offset value */ 
  DMA1STA = __builtin_dmaoffset(&ecan1msgBuf);  
  /* enable the channel */
  DMA1CONbits.CHEN = 1;
  
  /* Initialise the DMA channel 2 for ECAN Rx */
  /* clear the collision flags */
  DMACS0 = 0;
  /* setup channel 2 for peripheral indirect addressing mode 
  normal operation, word operation and select as Rx to peripheral */
  DMA2CON = 0x0020;
  /* setup the address of the peripheral ECAN1 (C1RXD) */ 
  DMA2PAD = (volatile unsigned int) &C1RXD;
  /* Set the data block transfer size of 8 */
  DMA2CNT = 7;
  /* automatic DMA Rx Initiation by DMA request */
  DMA2REQ = 0x0022; 
  /* DPSRAM atart adddress offset value */ 
  DMA2STA = __builtin_dmaoffset(&ecan1msgBuf);  
  /* enable the channel */
  DMA2CONbits.CHEN = 1;


  // disable DMA interrupts
  IEC0bits.DMA1IE = 0;
  IEC1bits.DMA2IE = 0;
}

