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

#include "debug.h"
#include "fram.h"
#include "general.h"
#include "leds.h"
#include "main.h"
#include "timer.h"

#include "../photon_3/Photon_3_0/params.pb.h"

#include <stdio.h>

/******************************************************************************/
/******************************************************************************/

extern volatile ParamsMessage params;

/******************************************************************************/
/******************************************************************************/

void debug_print_fram()
{
  char buf[256];
  uint16 addr, data;


  #define FRAM_SZ 0x2000

  for(addr=0; addr<FRAM_SZ; addr++){
    data = ReadWriteByteFram(addr, 0, FRAM_READ);
    sprintf(buf, "fram_addr:0x%04x : %02x\r\n", addr, data);
    debug_puts(buf);
  }

}

/******************************************************************************/
/******************************************************************************/
void debug_putc(char c)
{
  if(params.device.hw_rev < 2){
    if(U1STAbits.OERR){  U1STAbits.OERR = 0;}
    U1TXREG = c;
    while(U1STAbits.UTXBF){};
  }else{
    if(U2STAbits.OERR){  U2STAbits.OERR = 0;}
    U2TXREG = c;
    while(U2STAbits.UTXBF){};
  }
  ClrWdt();
}

/******************************************************************************/
/******************************************************************************/

void debug_puts(char *message)
{
  // Blocking version - use no interrupts
  uint16 i = 0;
  char c;
  while(1){
    c = message[i++];
    if(c == 0){break;}
    debug_putc(c);
  }
}

/******************************************************************************/
/******************************************************************************/

void debug_putl(char *message)
{
  debug_puts(message);
  debug_putc('\r');
  debug_putc('\n');
}

/******************************************************************************/
/******************************************************************************/

void InitDebug(void)
{
  uint16 brg;
  uint32 baudrate = 230400;

  switch(baudrate){
    case 2000000: brg = 4; break;
    case 921600: brg = 10; break;
    case 460800: brg = 21; break;
    case 230400: brg = 42; break;
    case 115200: brg = 86; break;
    case 9600: brg = 1041; break;
    default: while(1){};
  }

  if(params.device.hw_rev >= 2){
    TRISFbits.TRISF5 = 0;   // debug 2 - tx
    TRISFbits.TRISF4 = 0;   // debug 3 - gpio
    U2BRG = brg;
    U2MODE = 0;
    U2STA = 0;
    U2MODEbits.BRGH = 1;
    U2MODEbits.UARTEN = 1;
    U2STAbits.UTXEN = 1;
  }else{
    //TRISFbits.TRISF2 = 0;   // debug 0
    //TRISFbits.TRISF3 = 0;   // debug 1
    U1BRG = brg;
    U1MODE = 0;
    U1STA = 0;
    U1MODEbits.BRGH = 1;
    U1MODEbits.UARTEN = 1;
    U1STAbits.UTXEN = 1;
  }

}

/******************************************************************************/
/******************************************************************************/

void StartupPrint(void)
{
  debug_puts("\n\r\n\r\n\r\n\r");
  debug_puts("***********************************************************\n\r");
  debug_puts("CAN BOOTLOADER DEBUG PORT\n\r");
  debug_puts("***********************************************************\n\r");
  debug_puts("\n\r");
}

/******************************************************************************/
/******************************************************************************/


