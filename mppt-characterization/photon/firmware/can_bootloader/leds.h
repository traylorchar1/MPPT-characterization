/*******************************************************************************
leds.h
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

#ifndef LEDS_H
#define LEDS_H

#include "general.h"

/******************************************************************************/
/******************************************************************************/

#define LED_R_0         LATEbits.LATE5
#define LED_R_REG_0     LATE
#define LED_R_MASK_0    _LATE_LATE5_MASK
#define LED_R_TRIS_0    TRISEbits.TRISE5

#define LED_G_0         LATEbits.LATE7
#define LED_G_REG_0     LATE
#define LED_G_MASK_0    _LATE_LATE7_MASK
#define LED_G_TRIS_0    TRISEbits.TRISE7

#define LED_R_1         LATEbits.LATE7
#define LED_R_REG_1     LATE
#define LED_R_MASK_1    _LATE_LATE7_MASK
#define LED_R_TRIS_1    TRISEbits.TRISE7

#define LED_G_1         LATEbits.LATE6
#define LED_G_REG_1     LATE
#define LED_G_MASK_1    _LATE_LATE6_MASK
#define LED_G_TRIS_1    TRISEbits.TRISE6

/******************************************************************************/
/******************************************************************************/

//#define LED_G_ON()      {LED_G = ON;}
//#define LED_R_ON()      {LED_R = ON;}
inline void LED_G_ON(void);
inline void LED_R_ON(void);

/******************************************************************************/
/******************************************************************************/

//#define LED_G_OFF()     {LED_G = OFF;}
//#define LED_R_OFF()     {LED_R = OFF;}
inline void LED_G_OFF(void);
inline void LED_R_OFF(void);

/******************************************************************************/
/******************************************************************************/

//#define LED_G_TOG()     {LED_G_REG ^= LED_G_MASK;}
//#define LED_R_TOG()     {LED_R_REG ^= LED_R_MASK;}
inline void LED_G_TOG(void);
inline void LED_R_TOG(void);

/******************************************************************************/
/******************************************************************************/

void InitLEDs(void);
void enableLEDs(int state);

/******************************************************************************/
/******************************************************************************/

#endif
