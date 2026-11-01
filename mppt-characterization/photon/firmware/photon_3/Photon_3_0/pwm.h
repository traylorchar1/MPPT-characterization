/*******************************************************************************
pwm.h
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


#ifndef PWM_H 
#define PWM_H

#include "general.h"

/******************************************************************************/
/******************************************************************************/
//Function Definitions

void InitPWM(void);
void setPWMDutyCycle(uint16 D, uint8 state);
//void setPWMMaskStateDutyCycle(int16 D, uint16 mask);
//void initTimer2(void);
inline void enablePWM(void);
inline void disablePWM(void);
inline void Channel0On(void);
inline void Channel0Off(void);
inline void Channel1On(void);
inline void Channel1Off(void);
inline void ChannelOn(int16 channel);
inline void ChannelOff(int16 channel);

/******************************************************************************/
/******************************************************************************/
//Macro Definitions

#define PWM_OFF 	0b0001
#define PWM_ON		0b0010
#define PWM_DIODE	0b0100
#define PWM_SYNCH	0b1000

#define PWM_DEADTIME 500	//nanoseconds

/******************************************************************************/
/******************************************************************************/

struct PWMStruct{
	uint16 p;
	uint16 n;
	uint8 pwmMode;
};	

#endif
