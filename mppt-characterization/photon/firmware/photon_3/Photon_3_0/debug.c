/*******************************************************************************
   debug.c
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
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
   THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*******************************************************************************/

#include "cobs.h"
#include "debug.h"
#include "fifo.h"
#include "general.h"
#include "leds.h"
#include "main.h"
#include "timer.h"

#include "state.pb.h"
#include "params.pb.h"

#include <math.h>
#include <stdio.h>
#include <string.h>


/******************************************************************************/
/******************************************************************************/

// 921.6k -> 40e6 / (4 * 921600) - 1 = 9.85
#define BAUD_921k 10
  
// 2M -> 40e6 / (4 * 2e6) - 1 = 4 
#define BAUD_2M 4
//U1BRG = 4;

// 921.6k -> 40e6 / (4 * 115200) - 1 = 85.80
#define BAUD_115k 86

// 9600 -> 40e6 / (4 * 9600) - 1 = 1040.6
#define BAUD_9600 1041

/******************************************************************************/
/******************************************************************************/

extern uint8 ibuf[MAX(StateMessage_size, ParamsMessage_size)];
extern uint8 obuf[MAX(StateMessage_size, ParamsMessage_size)];
uint8 *obuf_ptr;
extern StateMessage state;
extern ParamsMessage params;

/******************************************************************************/
/******************************************************************************/

void _ISR _U1TXInterrupt(void)
{
  char c;
  if(U1STAbits.UTXBF) return;
  IFS0bits.U1TXIF = 0;
  c = *obuf_ptr++;
  U1TXREG = c;
  if(c == 0x00) IEC0bits.U1TXIE = 0;
} 

/******************************************************************************/
/******************************************************************************/

void _ISR _U1RXInterrupt(void)
{
  IFS0bits.U1RXIF = 0;
} 

/******************************************************************************/
/******************************************************************************/

void InitDebug(void)
{
  uint16 brg;

  switch(params.comm.debug_baudrate){
    case 2000000: brg = 4; break;
    case 921600: brg = 10; break;
    case 230400: brg = 43; break;
    case 115200: brg = 86; break;
    case 9600: brg = 1041; break;
    default: while(1){};
  }

  U1BRG = brg;
  U1MODE = 0;
  U1STA = 0;
  U1MODEbits.BRGH = 1;
  U1MODEbits.UARTEN = 1;
  U1STAbits.UTXEN = 1;
}

/******************************************************************************/
/******************************************************************************/


void SendStateMessage(void){
}

/******************************************************************************/
/******************************************************************************/


