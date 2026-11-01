/*******************************************************************************
general.c
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

#include "general.h"

#include "can.h"
#include "debug.h"
#include "fram.h"
#include "i2c.h"
#include "leds.h"
#include "main.h"
#include "oscillator.h"
#include "timer.h"

#include "../photon_3/Photon_3_0/params.pb.h"

#include <pb_decode.h>


#include <stdio.h>
#include <string.h>

/******************************************************************************/
/******************************************************************************/

static volatile uint16 InterruptState;

extern uint32 ReadLatch(uint16, uint16);
extern void JumpToUserCode(void);

extern volatile ParamsMessage params;
extern uint8 ibuf[ParamsMessage_size];

/******************************************************************************/
/******************************************************************************/

void STOP(void){
  while(1){
    ClrWdt();
  };
}

void Initialize(void){
  
  disableInterrupts();
  
  //InitDebugPins();
  InitLEDs();

  //turn off unused peripherals
  PMD1bits.QEI1MD = 1;    //disable QEI1
  PMD3bits.QEI2MD = 1;    //disable QEI2 

  PMD1bits.I2C1MD = 0;    //enable I2C1
  PMD3bits.I2C2MD = 1;    //disable I2C2

  PMD1bits.U1MD = 0;      //enable UART1
  PMD1bits.U2MD = 0;      //enable UART2

  PMD1bits.SPI1MD = 0;    //enable SPI1
  PMD1bits.SPI2MD = 0;    //enable SPI2

  PMD2bits.OC1MD = 1;     //disable Output Compare
  PMD2bits.OC2MD = 1;
  PMD2bits.OC3MD = 1;
  PMD2bits.OC4MD = 1;
 
  PMD2bits.IC1MD = 1;     //disable Input Capture
  PMD2bits.IC2MD = 1;
  PMD2bits.IC3MD = 1;
  PMD2bits.IC4MD = 1;
        
  params.device.hw_rev = ReadHwRevision();
 
  

  if(params.device.hw_rev >= 2){
    InitOsc(OSC);
  }else{
    InitOsc(XTAL);
  } 
  
  InitDebug();
  StartupPrint();
  InitFram();
  LoadParametersFromFram();
  
  StartCountdownTimer();

  LED_G_ON();

  InitCan();
        
  return;
}

/******************************************************************************/
/******************************************************************************/

void InitDebugPins(void){
  TRISFbits.TRISF2 = 0;   // debug 0
  TRISFbits.TRISF3 = 0;   // debug 1
  TRISFbits.TRISF5 = 0;   // debug 2
  TRISFbits.TRISF4 = 0;   // debug 3
  
  DEBUG_0_OFF();
  DEBUG_1_OFF();
  DEBUG_2_OFF();
  DEBUG_3_OFF();
}       

/******************************************************************************/
/******************************************************************************/

inline void disableInterrupts(void){
  InterruptState = DISICNT; 
  __asm__ volatile("disi #0x3FFF");
  SRbits.IPL = 7;
  return;
}       

/******************************************************************************/
/******************************************************************************/

inline void restoreInterrupts(void){
  if(InterruptState > 0){ __asm__ volatile("disi #0x3FFF"); }     
  else{                   enableInterrupts();  }
  return;
}

/******************************************************************************/
/******************************************************************************/               

inline void enableInterrupts(void){
  __asm__ volatile("disi #0x0000");
  SRbits.IPL = 0; 
  return;
}

/******************************************************************************/
/******************************************************************************/
/*
void _ISR _AddressError(void)
{
  disableInterrupts();
  INTCON1bits.ADDRERR = 0;        //Clear the trap flag
  LED_G_OFF();
  LED_R_OFF();
  while (1){
    disableInterrupts();
    delayMilliseconds(250);
    LED_G_TOG();
    ClrWdt();
  } 
}
*/
/******************************************************************************/
/******************************************************************************/
/*
void _ISR _StackError(void)
{
  disableInterrupts();    
  INTCON1bits.STKERR = 0;         //Clear the trap flag
  LED_G_OFF();
  LED_R_OFF();
  while(1){
    disableInterrupts();
    delayMilliseconds(250);
    //ClrWdt();
  }
}
*/
/******************************************************************************/
/******************************************************************************/
/*
void _ISR _MathError(void)
{
  disableInterrupts();    
  INTCON1bits.MATHERR = 0;        //Clear the trap flag
  LED_G_OFF();
  LED_R_OFF();
  while (1){
    disableInterrupts();
    delayMilliseconds(250);
    LED_R_TOG();
    //ClrWdt();
  }
}
*/
/******************************************************************************/
/******************************************************************************/
/*
void _ISR _DMACError(void)
{
  disableInterrupts();    
  INTCON1bits.DMACERR = 0;        //Clear the trap flag
  LED_G_OFF();
  LED_R_OFF();
  while (1){
    disableInterrupts();
    delayMilliseconds(250);
    LED_G_TOG();
    LED_R_TOG();
    //ClrWdt();
  }        
}
*/
/******************************************************************************/
/******************************************************************************/

void StartCountdownTimer(void){
  // 250ms timer
  InitTimer(1, 39052, TIMER_1to256_PRESCALE|TIMER_ON,
      IPL_1|INTERRUPT_DISABLED);
  IFS0bits.T1IF = 0;
  TMR1 = 0;
}

/******************************************************************************/
/******************************************************************************/

void StopCountdownTimer(void){
  TMR1 = 0;
  T1CONbits.TON = 0;
  IFS0bits.T1IF = 0;
}

/******************************************************************************/
/******************************************************************************/

static int countdown_clock = 0;

void CheckCountdownTimer(void){
  if(IFS0bits.T1IF){
    countdown_clock ++;
    LED_R_TOG();
    LED_G_TOG();
    debug_putl("tick");
    IFS0bits.T1IF = 0;
  }
  if(countdown_clock > 16){ // 4 seconds
    debug_putl("jump");
    delayMilliseconds(5);
    JumpToUserCode();
  }
}

/******************************************************************************/
/******************************************************************************/

int32 ReadHwRevision(void){
  uint16 rev = 0;
  uint16 i;
  
  //configure input ports
  TRISDbits.TRISD4 = 1;
  TRISDbits.TRISD5 = 1;
  TRISDbits.TRISD6 = 1;
  TRISDbits.TRISD7 = 1;

  //turn on the pull ups
  CNPU1bits.CN13PUE = 1;
  CNPU1bits.CN14PUE = 1;
  CNPU1bits.CN15PUE = 1;
  CNPU2bits.CN16PUE = 1;
  
  //allow voltage to stabilize
  for(i=0xFFFF; i>0; i--){nop();}
  
  //read the value and put it in addr     
  rev =  PORTDbits.RD4;
  rev |= PORTDbits.RD5 << 1;
  rev |= PORTDbits.RD6 << 2;
  rev |= PORTDbits.RD7 << 3;
  rev = ~rev;
  rev &= 0x0F;
  
  //turn off the pull ups - save power
  CNPU1bits.CN13PUE = 0;
  CNPU1bits.CN14PUE = 0;
  CNPU1bits.CN15PUE = 0;
  CNPU2bits.CN16PUE = 0;
  
  return rev;
}
/******************************************************************************/
/******************************************************************************/

uint32 readChannelNum(void){
  return 0;
}

/******************************************************************************/
/******************************************************************************/

#define MSG_OFFSET  4

void LoadParametersFromFram(void){
  union u_t msg_size;
  uint16 i;
  char buf[0x100];
  pb_istream_t stream;

  // First measure the size of the message
  for(i=0; i<4; i++){
    msg_size.u8[i] = ReadWriteByteFram(i, 0, FRAM_READ);
  }
  #ifdef DEBUG_EN
    sprintf(buf, "msg_sz\t%ld", msg_size.u32);
    debug_putl(buf);
  #endif

  // wipe fram
  for(i=0; i<255; i++){
    //ReadWriteByteFram(i, 0, FRAM_WRITE); 
  }

  // Load the message into ibuf
  memset(ibuf, 0, ParamsMessage_size);
  for(i=0; i<msg_size.u32; i++){
    ibuf[i] = ReadWriteByteFram((i+MSG_OFFSET), 0, FRAM_READ); 
  }
  

  // Parse the message if anythong is in the FRAM
  if(msg_size.u32 > 0){
    stream = pb_istream_from_buffer(ibuf, msg_size.u32);
    pb_decode(&stream, ParamsMessage_fields, &params);
  }else{
    //pb_message_set_to_defaults(ParamsMessage_fields, &params);
    params.comm.can_base_addr = 0x600;
    params.comm.can_bitrate = 500000;
    params.device.serial_number = -1;
  }
  
  sprintf(buf, "msg decoded");
  debug_putl(buf);
  sprintf(buf, "base_addr: %lx", params.comm.can_base_addr);
  debug_putl(buf);
  sprintf(buf, "can_bitate: %ld", params.comm.can_bitrate);
  debug_putl(buf);
  sprintf(buf, "debug_baudrate: %ld", params.comm.debug_baudrate);
  debug_putl(buf);
  sprintf(buf, "serial_number: %ld", params.device.serial_number);
  debug_putl(buf);

  params.device.hw_rev = ReadHwRevision();

}

/******************************************************************************/
/******************************************************************************/




