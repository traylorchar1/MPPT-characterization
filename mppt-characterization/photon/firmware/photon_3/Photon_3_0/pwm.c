/*******************************************************************************
pwm.c
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

#include "pwm.h"
#include "main.h"
#include "can.h"
#include "fpga.h"
#include "leds.h"
#include "timer.h"
#include <smpspwm.h>

#include "params.pb.h"

/******************************************************************************/
/******************************************************************************/

extern volatile ParamsMessage params;

/******************************************************************************/
/******************************************************************************/

void _ISR _PWM1Interrupt (void){
  if(PWMCON1bits.FLTSTAT){
    PWMCON1bits.FLTIEN = 0;
  }
  if(PWMCON1bits.CLSTAT){
    PWMCON1bits.CLIEN = 0;
  }
  if(PWMCON2bits.FLTSTAT){
    PWMCON2bits.FLTIEN = 0;
  }
  if(PWMCON2bits.CLSTAT){
    PWMCON2bits.CLIEN = 0;
  }

  IFS5bits.PWM1IF = 0;
}

/******************************************************************************/
/******************************************************************************/

void setPWMDutyCycle(uint16 D, uint8 state){
  return;
}

/******************************************************************************/
/******************************************************************************/	

inline void Channel0On(void){
  if(params.device.hw_rev >= PHOTON_3_2){
    EnableFpgaMppt0();
  }else{
    IOCON1bits.OVRDAT = 0;	//override to zero
    IOCON1bits.OVRENH = 1;	//top output override
    IOCON1bits.OVRENL = 0;	//bottom output enabled
    PTCONbits.PTEN = 1;
  }
}

/******************************************************************************/
/******************************************************************************/	
	
inline void Channel0Off(void){
  if(params.device.hw_rev >= PHOTON_3_2){
    DisableFpgaMppt0();
  }else{
    IOCON1bits.OVRDAT = 0;	//override to zero
    IOCON1bits.OVRENH = 1;	//top output override
    IOCON1bits.OVRENL = 1;	//bottom output disabled
    PTCONbits.PTEN = 1;
  }
}

/******************************************************************************/
/******************************************************************************/	
	
inline void Channel1On(void){
  if(params.device.hw_rev >= PHOTON_3_2){
    EnableFpgaMppt1();
  }else{
    IOCON2bits.OVRDAT = 0;	//override to zero
    IOCON2bits.OVRENH = 1;	//top output override
    IOCON2bits.OVRENL = 0;	//bottom output enabled
  }
}

/******************************************************************************/
/******************************************************************************/	
	
inline void Channel1Off(void){
  if(params.device.hw_rev >= PHOTON_3_2){
    DisableFpgaMppt1();
  }else{
    IOCON2bits.OVRDAT = 0;	//override to zero
    IOCON2bits.OVRENH = 1;	//top output override
    IOCON2bits.OVRENL = 1;	//bottom output disabled
  }
}

/******************************************************************************/
/******************************************************************************/	

inline void ChannelOn(int16 channel){
  if(channel == 0){ Channel0On();}
  if(channel == 1){ Channel1On();}
}

/******************************************************************************/
/******************************************************************************/	

inline void ChannelOff(int16 channel){
  if(channel == 0){ Channel0Off();}
  if(channel == 1){ Channel1Off();}
}

/******************************************************************************/
/******************************************************************************/	

inline void enablePWM(void){
}

/******************************************************************************/
/******************************************************************************/

inline void disablePWM(void){
  Channel0Off();
  Channel1Off();  
}		

/******************************************************************************/
/******************************************************************************/

void InitPWM(void){
  CloseSmpsPWM();               //reset module
	
  PTCONbits.PTEN = 0;           // Disable PWM module
  PTCONbits.PTSIDL = 0;         // Run in idle mode
  PTCON2bits.PCLKDIV = 0;       // Maximum speed
  PTPER = PWM_TIMEBASE;
  MDC = 0;

  // Setting up module 1
  IOCON1bits.OVRDAT = 0;	// override to zero
  IOCON1bits.PENH = 0;	        // top module disabled
  IOCON1bits.OVRENL = 1;        // bottom output override
  IOCON1bits.PENL = 1;	        // bottom module enabled

  PHASE1 = PWM_TIMEBASE;
  PDC1 = 0;

  // Setting up current limit
  IOCON1bits.CLDAT = 1;         // Sets output to low on current limit
  IOCON1bits.FLTDAT = 0;        // Sets output to low on a fault
  
  FCLCON1bits.IFLTMOD = 0;      // Normal fault mode
  FCLCON1bits.CLSRC = 3;        // CMP4 is source for current limit
  FCLCON1bits.FLTSRC = 0;       // CMP1 is source for fault limit
  FCLCON1bits.CLPOL = 0;        // Active high current limit source
  FCLCON1bits.FLTPOL = 0;       // Active high fault source
  FCLCON1bits.CLMOD = 0;        // Enable cycle by cycle current limit mode
  FCLCON1bits.FLTMOD = 1;       // Enable Cycle by cycle fault mode
  
  PWMCON1bits.ITB = 1;          // Setting independent timebase
  PWMCON1bits.XPRES = 1;        // Setting current reset mode
  PWMCON1bits.FLTIEN = 0;       // Enable fault interrupt
  PWMCON1bits.CLIEN = 0;        // Enable current limit interrupt

  // Leading edge blanking
  LEBDLY1bits.LEB = 127;        // Blanking period of 1us
  LEBCON1bits.PHR = 0;          // Rising edge of PWMxH is blanked
  LEBCON1bits.PHF = 0;          // Falling edge of PWMxH is blanked
  LEBCON1bits.PLR = 1;          // Rising edge of PWMxL is blanked
  LEBCON1bits.PLF = 0;          // Falling edge of PWMxL is blanked
  LEBCON1bits.FLTLEBEN = 1;     // Enable fault LEB for this source
  LEBCON1bits.CLLEBEN = 1;      //Enable current limit LEB

  IOCON2 = IOCON1;
  PHASE2 = PHASE1;
  PDC2 = PDC1;
  IOCON2 = IOCON1;
  FCLCON2 = FCLCON1;
  FCLCON2bits.CLSRC = 1;        // CMP2 is source for current limit
  FCLCON2bits.FLTSRC = 2;       // CMP3 is source for fault limit
  PWMCON2 = PWMCON1;
  LEBCON2 = LEBCON1;
  LEBDLY2 = LEBDLY1;

  IEC5bits.PWM1IE = 0;          // Enable the interupt
  IEC5bits.PWM2IE = 0;          // Enable the interupt
  
  disableOutput();
	
}

/******************************************************************************/
/******************************************************************************/

