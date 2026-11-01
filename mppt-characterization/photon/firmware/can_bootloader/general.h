/*******************************************************************************
general.h
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

#ifndef GENERAL_H
#define GENERAL_H

/******************************************************************************/
/******************************************************************************/

#include "../dspic_include/p33FJ64GS606.h"
#include "../dspic_include/Generic.h"

#include <stdlib.h>
#include <stddef.h>

/******************************************************************************/
/******************************************************************************/

#define DO_PRAGMA(x) _Pragma (#x)
#define TODO(x) DO_PRAGMA(message ("\e[1;36mTODO: \e[1;93m" #x "\e[0m"))

/******************************************************************************/
/******************************************************************************/

// Integer types
typedef unsigned char       uint8;
typedef unsigned int        uint16;
typedef unsigned long       uint32;

typedef signed char         int8;
typedef signed short        int16;
typedef signed long         int32;
typedef signed long long    int64;

/******************************************************************************/
/******************************************************************************/

#define UINT8MAX  ((uint8)0xff)
#define UINT16MAX ((uint16)0xffff)
#define UINT32MAX ((uint32)0xffffffff)
#define UINT64MAX ((uint64)0xffffffffffffffff)

#define INT8MAX  ((int8)127)
#define INT16MAX ((int16)32767)
#define INT32MAX ((int32)2147483647)
#define INT64MAX ((int64)9223372036854775807)

#define INT8MIN  ((int8)-128)
#define INT16MIN ((int16)−32768)
#define INT32MIN ((int32)−2147483648)
#define INT64MIN ((int64)-9223372036854775808)

/******************************************************************************/
/******************************************************************************/

// Floating point types
typedef float               float32;
typedef double              float64;
typedef long double         float128;

/******************************************************************************/
/******************************************************************************/

//Function types
typedef void(*voidfxn)(void);

/******************************************************************************/
/******************************************************************************/

union u_t{
        uint32 u32;
        uint32 lu;
        float f;
        int32 ls;
        struct {
                uint16 lu;
                uint16 hu;
        }su;
        struct {
                int16 ls;
                int16 hs;
        }ss;
        uint8 cu[4];
        char cs[4];
        uint8 u8[4];
        char s8[4];
        uint16 u16[2];
        int16 s16[2];
};

/******************************************************************************/
/******************************************************************************/

//Constant Definitions

/******************************************************************************/
/******************************************************************************/

#define PHOTON_3_0 (0)
#define PHOTON_3_1 (1)

/******************************************************************************/
/******************************************************************************/

#define TRUE      (1)
#define FALSE     (0)
#define True      TRUE
#define False     FALSE
#define T         TRUE
#define F         FALSE

#define DISABLED  FALSE
#define ENABLED   TRUE

#define INPUT     TRUE
#define OUTPUT    FALSE

#define ON        TRUE
#define OFF       FALSE

#define COMPLEMENTARY   FALSE
#define INDEPENDENT     TRUE

/******************************************************************************/
/******************************************************************************/

#ifdef _ISR
#undef _ISR
#endif

#ifdef _ISRFAST
#undef _ISRFAST
#endif

#define _ISR __attribute__((interrupt, auto_psv))
#define _ISR_DEF __attribute__((__interrupt__))
#define _ISRFAST __attribute__((interrupt, shadow, auto_psv))
#define _PERSISTENT __attribute__((persistent))
#define _NEAR __attribute__((near))
#define _DMA_SPACE __attribute__((space(dma),aligned(32)))

/******************************************************************************/
/******************************************************************************/

inline void disableInterrupts(void);    
inline void restoreInterrupts(void);
inline void enableInterrupts(void);

/******************************************************************************/
/******************************************************************************/
                
#define nop() {__asm__ volatile("nop");}

/******************************************************************************/
/******************************************************************************/

#define reset()                 {__asm__ volatile ("reset");}

/******************************************************************************/
/******************************************************************************/

int32 ReadHwRevision(void);

/******************************************************************************/
/******************************************************************************/

void Blink(int16 blinks);
void STOP(void);

/******************************************************************************/
/******************************************************************************/

//Initialization functions
void Initialize(void);
void InitDebugPins(void);

void StartCountdownTimer(void);
void StopCountdownTimer(void);
void CheckCountdownTimer(void);

void LoadParametersFromFram(void);

/******************************************************************************/
/******************************************************************************/

#define DEBUG_0_PIN         LATFbits.LATF2
#define DEBUG_1_PIN         LATFbits.LATF3
#define DEBUG_2_PIN         LATFbits.LATF5
#define DEBUG_3_PIN         LATFbits.LATF4

#define DEBUG_0_TOG()       {DEBUG_0_PIN ^= TRUE;}
#define DEBUG_1_TOG()       {DEBUG_1_PIN ^= TRUE;}
#define DEBUG_2_TOG()       {DEBUG_2_PIN ^= TRUE;}
#define DEBUG_3_TOG()       {DEBUG_3_PIN ^= TRUE;}

#define DEBUG_0_ON()        {DEBUG_0_PIN = TRUE;}
#define DEBUG_1_ON()        {DEBUG_1_PIN = TRUE;}
#define DEBUG_2_ON()        {DEBUG_2_PIN = TRUE;}
#define DEBUG_3_ON()        {DEBUG_3_PIN = TRUE;}

#define DEBUG_0_OFF()       {DEBUG_0_PIN = FALSE;}
#define DEBUG_1_OFF()       {DEBUG_1_PIN = FALSE;}
#define DEBUG_2_OFF()       {DEBUG_2_PIN = FALSE;}
#define DEBUG_3_OFF()       {DEBUG_3_PIN = FALSE;}

/******************************************************************************/
/******************************************************************************/

//traps
//void _ISR_DEF _AddressError(void);
//void _ISR_DEF _StackError(void);
//void _ISR_DEF _MathError(void);
//void _ISR_DEF _DMACError(void);

/******************************************************************************/
/******************************************************************************/

#endif

