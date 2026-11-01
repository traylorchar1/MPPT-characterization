/*******************************************************************************
timer.h
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

#ifndef TIMER_H
#define TIMER_H

/******************************************************************************/
/******************************************************************************/

#include "general.h"

/******************************************************************************/
/******************************************************************************/

#define interrupts_per_second 	35

#define TIMER_INTERNAL_CLOCK	0x0000
#define TIMER_EXTERNAL_CLOCK	0x0002
#define TIMER_SYNC				0x0004	//only timer 1
#define TIMER_32BIT_MODE		0x0008	//only timers 2/3, 4/5
#define TIMER_1to1_PRESCALE		0x0000
#define TIMER_1to8_PRESCALE		0x0010
#define TIMER_1to64_PRESCALE	(0x0020)
#define TIMER_1to256_PRESCALE	0x0030
#define TIMER_GATED_MODE		0x0040
#define TIMER_STOP_IN_IDLE		0x2000
#define TIMER_ON				(0x8000)

#define IPL_0				0
#define IPL_1				1
#define IPL_2				2
#define IPL_3				3
#define IPL_4				4
#define IPL_5				5
#define IPL_6				6
#define IPL_7				7
#define INTERRUPT_ENABLED	0x80
#define INTERRUPT_DISABLED	0x00

/******************************************************************************/
/******************************************************************************/

void InitTimer(uint8 channel, uint16 period, uint16 option1, uint8 option2);

void delayMilliseconds(int milliseconds);
void delay1Millisecond(void);
void delay100Microseconds(void);
void delay10Microseconds(void);
void delay1Microsecond(void);
void delayMicroseconds(unsigned int microseconds);

/******************************************************************************/
/******************************************************************************/

#endif
