/*******************************************************************************
comparator.c
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

#include "comparator.h"
#include "general.h"
#include "pwm.h"
#include "../../dspic_include/smpscmp.h"
#include "leds.h"
#include "main.h"
#include "debug.h"
#include "timer.h"
#include "params.pb.h"
#include "state.pb.h"


/******************************************************************************/
/******************************************************************************/

#define DACREF  512

//160kHz

/******************************************************************************/
/******************************************************************************/

extern volatile StateMessage state;
extern volatile ParamsMessage params;

/******************************************************************************/
/******************************************************************************/

void _ISR _CMP1Interrupt (void){
  IFS1bits.AC1IF = 0;   //clear interrupt flag
}

/******************************************************************************/
/******************************************************************************/

void _ISR _CMP2Interrupt (void){
  // Current limit channel 1

  // Check to ensure we've waited at least the minimum period (6.25us, 160kHz)
  if(IFS1bits.T5IF == 0){
    IFS6bits.AC2IF = 0; //clear interrupt flag
    return;
  }
  PWMCON2bits.XPRES = 0;
  PWMCON2bits.XPRES = 1;
  PWMCON2bits.XPRES = 0;
  PWMCON2bits.XPRES = 1;
  PWMCON2bits.XPRES = 0;
  state.chan[1].period = TMR5;
  TMR5 = 0;
  IFS1bits.T5IF = 0;
  IFS6bits.AC2IF = 0;   //clear interrupt flag
}

/******************************************************************************/
/******************************************************************************/

void _ISR _CMP3Interrupt (void){
  IFS6bits.AC3IF = 0;   //clear interrupt flag
}

/******************************************************************************/
/******************************************************************************/

void _ISR _CMP4Interrupt (void){
  // Current limit channel 0
  
  // Check to ensure we've waited at least the minimum period (6.25us, 160kHz)
  if(IFS0bits.T2IF == 0){
    IFS6bits.AC4IF = 0; //clear interrupt flag
    return;
  }
  PWMCON1bits.XPRES = 0;
  PWMCON1bits.XPRES = 1;
  PWMCON1bits.XPRES = 0;
  PWMCON1bits.XPRES = 1;
  PWMCON1bits.XPRES = 0;
  state.chan[0].period = TMR2;
  TMR2 = 0;
  IFS0bits.T2IF = 0;
  IFS6bits.AC4IF = 0;   //clear interrupt flag
}

/******************************************************************************/
/******************************************************************************/

void InitComparator(void){

  ADPCFGbits.PCFG12 = 0;        // AN12 is configured as analog input
  ADPCFGbits.PCFG13 = 0;        // AN13 is configured as analog input
  ADPCFGbits.PCFG14 = 0;        // AN14 is configured as analog input
  ADPCFGbits.PCFG15 = 0;        // AN15 is configured as analog input

  // Configure module 1 - Channel 0 mosfet current
  CMPCON1bits.RANGE = 1;        // Range mode 0-> 1.2V, 1-> 1.65V.
  CMPCON1bits.DACOE = 0;        // DAC output disabled.
  CMPCON1bits.INSEL = 3;        // D input selected.
  CMPCON1bits.EXTREF = 0;       // Internal reference selected.
  CMPCON1bits.CMPSIDL = 0;      // Active during idle.
  CMPCON1bits.CMPPOL = 0;       // Active high polarity.
  CMPCON1bits.CMPON = 1;        // Module on.
  
  // Configure module 3 - Channel 1 mosfet current
  CMPCON3bits.RANGE = 1;        // Range mode 0-> 1.2V, 1-> 1.65V.
  CMPCON3bits.DACOE = 0;        // DAC output disabled.
  CMPCON3bits.INSEL = 3;        // D input selected.
  CMPCON3bits.EXTREF = 0;       // Internal reference selected.
  CMPCON3bits.CMPSIDL = 0;      // Active during idle.
  CMPCON3bits.CMPPOL = 0;       // Active high polarity.
  CMPCON3bits.CMPON = 1;        // Module on.
  
  // 133 bits per amp
  CMPDAC1 = 100;                // Set DAC reference level
  CMPDAC3 = 100;                // Set DAC reference level

  // Configure module 4 - Channel 0 voltage
  CMPCON4bits.RANGE = 1;        // Range mode 0-> 1.2V, 1-> 1.65V.
  CMPCON4bits.DACOE = 0;        // DAC output disabled.
  CMPCON4bits.INSEL = 3;        // D input selected.
  CMPCON4bits.EXTREF = 0;       // Internal reference selected.
  CMPCON4bits.CMPSIDL = 0;      // Active during idle.
  CMPCON4bits.CMPPOL = 0;       // Active low polarity.
  CMPCON4bits.CMPON = 1;        // Module on.
  

  // Configure module 2 - Channel 1 voltage
  CMPCON2bits.RANGE = 1;        // Range mode 0-> 1.2V, 1-> 1.65V.
  CMPCON2bits.INSEL = 3;        // D input selected.
  CMPCON2bits.EXTREF = 0;       // Internal reference selected.
  CMPCON2bits.CMPSIDL = 0;      // Active during idle.
  CMPCON2bits.CMPPOL = 0;       // Active low polarity.
  CMPCON2bits.CMPON = 1;        // Module on.
  
  // DAC output enabled. This DAC output is used for the voltage comparator
  // on Photon 3.1 and later hardware.
  CMPCON2bits.DACOE = 1;

  // 6.157 bits per volt
  CMPDAC4 = DACREF;             // Set DAC reference level
  CMPDAC2 = DACREF;             // Set DAC reference level

  ConfigIntSmpsCMP1(CMP1_INT_DIS, CMP1_INT_PR1);  //disabled, pri=1
  ConfigIntSmpsCMP3(CMP3_INT_DIS, CMP3_INT_PR1);  //disabled, pri=1
  
  ConfigIntSmpsCMP4(CMP4_INT_EN, CMP4_INT_PR5);   //enabled, pri=5
  ConfigIntSmpsCMP2(CMP2_INT_EN, CMP2_INT_PR5);   //enabled, pri=5

}

/******************************************************************************/
/******************************************************************************/

