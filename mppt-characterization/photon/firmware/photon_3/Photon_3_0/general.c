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

#include "adc.h"
#include "can.h"
#include "comparator.h"
#include "debug.h"
#include "fpga.h"
#include "fram.h"
#include "i2c.h"
#include "leds.h"
#include "main.h"
#include "oscillator.h"
#include "packet.h"
#include "pwm.h"
#include "queue.h"
#include "safety.h"
#include "timer.h"
#include "thermistor.h"
#include "tracker.h"

#include "params.pb.h"
#include "state.pb.h"

#include <pb_decode.h>


/******************************************************************************/
/******************************************************************************/

extern volatile StateMessage state;
extern volatile ParamsMessage params;

static volatile uint16 InterruptState;

extern uint8 ibuf[MAX(StateMessage_size, ParamsMessage_size)];
extern uint8 obuf[MAX(StateMessage_size, ParamsMessage_size)];

extern uint32 ReadLatch(uint16, uint16);

/******************************************************************************/
/******************************************************************************/

void STOP(void){
  while(1){
    ClrWdt();
  };
}

/******************************************************************************/
/******************************************************************************/

void DebugToggle(int16 togs){
  int i;
  DEBUG_3_OFF();
  for(i=0; i<togs; i++){
    delay1Microsecond();
    DEBUG_3_ON();
    delay1Microsecond();
    DEBUG_3_OFF();
  }
  delay10Microseconds();
}

/******************************************************************************/
/******************************************************************************/

void Initialize(void){
  
  disableInterrupts();
  
  InitStructures();

  InitLEDs();
  InitRelays();

  //turn off unused peripherals
  PMD1bits.QEI1MD = 1;    //disable QEI1
  PMD3bits.QEI2MD = 1;    //disable QEI2 

  PMD1bits.I2C1MD = 0;    //enable I2C1
  PMD3bits.I2C2MD = 1;    //disable I2C2

  PMD1bits.U1MD = 1;      //disable UART1
  PMD1bits.U2MD = 1;      //disable UART2

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

  // Configure oscilator
  if(params.device.hw_rev >= PHOTON_3_2){
    InitOsc(OSC);
  }else{
    InitOsc(XTAL);
  }

  InitSafety();

  //Blink(2);

  InitDebugPins();
  //InitDebug();
  InitFram();
  LED_G_ON();

  
  // Configure FPGA
  if(params.device.hw_rev >= PHOTON_3_2){
    InitFpga();
    ProgramFpga();
  }

  // 216us for SendStateMessageData
  //InitTimer(2, 8640, TIMER_1to1_PRESCALE|TIMER_ON, IPL_2|INTERRUPT_ENABLED);
  
  // 50hz for RunTracker
  //InitTimer(3, 12500, TIMER_1to64_PRESCALE|TIMER_ON, IPL_1|INTERRUPT_ENABLED);
  // 1kHz scheduler
  InitTimer(3, 625, TIMER_1to64_PRESCALE|TIMER_ON, IPL_1|INTERRUPT_ENABLED);
  
  // 200Hz for ControlVoltages
  //InitTimer(5, 3125, TIMER_1to64_PRESCALE|TIMER_ON, IPL_6|INTERRUPT_ENABLED);
  
  // Minimum on time timers
  InitTimer(2, MIN_PERIOD, TIMER_1to1_PRESCALE|TIMER_ON, IPL_1|INTERRUPT_DISABLED);
  InitTimer(5, MIN_PERIOD, TIMER_1to1_PRESCALE|TIMER_ON, IPL_1|INTERRUPT_DISABLED);
    

  InitThermistor();
  InitI2C(BRG_100K);
  InitAdc();
  
  InitEnable();   
  InitTracker();
  InitCAN();
  sendDebugPacket();

  if(params.device.hw_rev >= PHOTON_3_2){
    PTCONbits.PTEN = 0;
  }else{
    InitComparator();
    InitPWM();
  }

  enableInterrupts();
        
  QueueInitialize();
        
  LED_R_OFF();
  LED_G_OFF();
  
  return;
}

/******************************************************************************/
/******************************************************************************/

void Blink(int16 blinks){
  int i;
  LED_G_OFF();
  LED_R_OFF();
  for(i=0; i<blinks; i++){
    delayMilliseconds(250);
    LED_G_TOG();
    LED_R_TOG();
  }
  LED_G_OFF();
  LED_R_OFF();
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

void InitEnable(void){
  disableOutput();
}       


/******************************************************************************/
/******************************************************************************/

void InitRelays(void){
  SetPrechargeRelayState(OFF);
  SetMainRelayState(OFF);

  if(params.device.hw_rev >= PHOTON_3_2){
    nop();
  }else{
    // Initialize relay outputs
    TRISDbits.TRISD1 = 0;
    TRISDbits.TRISD2 = 0;
  }
}

/******************************************************************************/
/******************************************************************************/

void SetMainRelayState(int relay_state){
  if(relay_state==OFF){
    if(params.device.hw_rev >= PHOTON_3_2){
      LATEbits.LATE3 = 0; // DSP_GPIO_3
    }else{
      LATDbits.LATD1 = 0;
    }
    state.main = RelayEnum_OPEN;
  }
  if(relay_state==ON){
    if(params.device.hw_rev >= PHOTON_3_2){
      LATEbits.LATE3 = 1; // DSP_GPIO_3
    }else{
      LATDbits.LATD1 = 1;
    }
    state.main = RelayEnum_CLOSED;
  }
}
/******************************************************************************/
/******************************************************************************/

void SetPrechargeRelayState(int relay_state){
  if(relay_state==OFF){
    if(params.device.hw_rev >= PHOTON_3_2){
      LATEbits.LATE1 = 0; // DSP_GPIO_2
    }else{
      LATDbits.LATD2 = 0;
    }
    state.precharge = RelayEnum_OPEN;
  }
  if(relay_state==ON){
    if(params.device.hw_rev >= PHOTON_3_2){
      LATEbits.LATE1 = 1; // DSP_GPIO_2
    }else{
      LATDbits.LATD2 = 1;
    }
    state.precharge = RelayEnum_CLOSED;
  }
}

/******************************************************************************/
/******************************************************************************/

void Precharge(void){
  long i = 0;
  LED_R_ON();
  LED_G_OFF();
  SetPrechargeRelayState(ON); 
  for(i=0; i<0x5000000; i++){
    ClrWdt();
  }
  LED_G_ON();
  SetMainRelayState(ON);
  for(i=0; i<0x500000; i++){
    ClrWdt();
  }
  LED_R_OFF();
  SetPrechargeRelayState(OFF); 
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

void InitStructures(void){
  int16 c;

  // Initialize the protobufs
  LoadParametersFromFram();
  params.device.hw_rev = ReadHwRevision();
  
  state.chan_count = 2;
  
  state.has_sequence = True;
  state.has_batt_v_raw = True;
  state.has_batt_i_raw = True;
  state.has_temp_ambient = True;
  state.has_safety = True;
  state.has_control = True;
  //state.has_precharge = True;
  //state.has_main = True;
  state.has_loops = True;
  //state.has_precharge_loops = True;
  //state.has_precharge_adc = True;
  state.has_batt_p_raw = True;
  state.has_batt_p_last_raw = True;
  state.has_dbg = True;
  //state.has_t_raw = True;
  state.has_alg = True;
  //state.has_batt_v = True;
  //state.has_batt_i = True;
  //state.has_fpga_fault = True;
  //state.has_max_dac = True;
  //state.has_n_burst_pkts = True;

  for(c=0; c<state.chan_count; c++){
    state.chan[c].has_v_raw = True;
    state.chan[c].has_v_raw_last_change = True;
    state.chan[c].has_v_goal_raw = True;
    state.chan[c].has_v_error_raw = True;
    //state.chan[c].has_p_term = True;
    //state.chan[c].has_i_term = True;
    state.chan[c].has_i_raw = True;
    state.chan[c].has_p_raw = True;
    state.chan[c].has_p_last_raw = True;
    state.chan[c].has_temp = True;
    state.chan[c].has_dac = True;
    state.chan[c].has_dir = True;
    state.chan[c].has_dbg = True;
    //state.chan[c].has_period = True;
    //state.chan[c].has_t_raw = True;
    //state.chan[c].has_v_last = True;
    //state.chan[c].has_i_last = True;
    //state.chan[c].has_v_delta = True;
    //state.chan[c].has_i_delta = True;
    //state.chan[c].has_iv = True;
    //state.chan[c].has_didv = True;
    //state.chan[c].has_v = True;
    //state.chan[c].has_i = True;
    state.chan[c].has_voc_raw = True;
    state.chan[c].has_vsc_raw = True;
    state.chan[c].has_p_last_change_raw = True;
  }
}

/******************************************************************************/
/******************************************************************************/

#define MSG_OFFSET  4

void LoadParametersFromFram(void){
  union u_t msg_size;
  uint16 i;
  pb_istream_t stream;

  // First measure the size of the message
  for(i=0; i<4; i++){
    msg_size.u8[i] = ReadWriteByteFram(i, 0, FRAM_READ);
  }

  // Load the message into ibuf
  memset(ibuf, 0, ParamsMessage_size);
  for(i=0; i<msg_size.u32; i++){
    ibuf[i] = ReadWriteByteFram((i+MSG_OFFSET), 0, FRAM_READ); 
  }

  // Parse the message
  stream = pb_istream_from_buffer(ibuf, msg_size.u32);
  pb_decode(&stream, ParamsMessage_fields, &params);

}
/******************************************************************************/
/******************************************************************************/

void _ISR _AddressError(void)
{
  disableOutput();
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

/******************************************************************************/
/******************************************************************************/

void _ISR _StackError(void)
{
  disableOutput();
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

/******************************************************************************/
/******************************************************************************/

void _ISR _MathError(void)
{
  disableOutput();
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

/******************************************************************************/
/******************************************************************************/

void _ISR _DMACError(void)
{
  disableOutput();
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



