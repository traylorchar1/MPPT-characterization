/*******************************************************************************
oscillator.h
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

#ifndef OSC_H
#define OSC_H

/******************************************************************************/
/******************************************************************************/

#include "general.h"

/******************************************************************************/
/******************************************************************************/

#define OSC_FIN_XTAL 	 8000000
#define OSC_FIN_OSC 	 16000000
#define OSC_FFRC	 7370000
#define OSC_FOSC 	80000000
#define OSC_FCY (OSC_FOSC/2)
//#define OSC_PLLDIV ((OSC_FOSC*4)/(OSC_FIN)-2)

#define XTAL	        0
#define FRC		1
#define OSC		2

/******************************************************************************/
/******************************************************************************/

#define OSC_FRC				0b000
#define OSC_FRC_PLL			0b001
#define OSC_PRI				0b010
#define OSC_PRI_PLL			0b011
#define OSC_SEC				0b100
#define OSC_LOW_PWR			0b101
#define OSC_FRC_DIV_16		0b110
#define OSC_FRC_DIV_N		0b111

#define OSC_START_SWITCHING 0x01

/******************************************************************************/
/******************************************************************************/

void InitOsc(uint8 crystal);
void _ISR_DEF _OscillatorFail(void);

/******************************************************************************/
/******************************************************************************/

#endif


