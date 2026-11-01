/*******************************************************************************
oscillator.c
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


#include "oscillator.h"

/******************************************************************************/
/******************************************************************************/

void InitOsc(uint8 crystal){
  int32 plldivisor = 0;
  switch(crystal){
    case XTAL:
      plldivisor = OSC_FOSC * 4;
      plldivisor /= OSC_FIN_XTAL;
      plldivisor -= 2;
        
      // Configure PLL prescaler, PLL postscaler, PLL divisor
      PLLFBD=plldivisor;                        
      CLKDIVbits.PLLPOST = 0;   // N1=2
      CLKDIVbits.PLLPRE = 0;    // N2=2

      // Initiate Clock Switch to Primary Oscillator with PLL
      __builtin_write_OSCCONH(0x03);

      // Start oscillator switching sources
      __builtin_write_OSCCONL(0x01);
        
      // Wait for Clock switch to occur
      while(OSCCONbits.LOCK!=1) {};
      break;
                
   case FRC:
      STOP();
      break;     
   case OSC:
      // External Oscillator mode
      plldivisor = OSC_FOSC * 4;
      plldivisor /= OSC_FIN_OSC;
      plldivisor -= 2;
        
      // Configure PLL prescaler, PLL postscaler, PLL divisor
      PLLFBD=plldivisor;                        
      CLKDIVbits.PLLPOST = 0;   // N1=2
      CLKDIVbits.PLLPRE = 0;    // N2=2

      // Initiate Clock Switch to Primary Oscillator with PLL
      __builtin_write_OSCCONH(0x03);

      // Start oscillator switching sources
      __builtin_write_OSCCONL(0x01);
        
      // Wait for Clock switch to occur
      while(OSCCONbits.LOCK!=1) {};

      break;     
   default:
      STOP();   
      break;
  }
  
  // FRC provides input for Auxiliary PLL (x16)  
  ACLKCONbits.FRCSEL = 1;  
  
  // Auxiliary Ocillator provides clock source for PWM & ADC
  ACLKCONbits.SELACLK = 1;

  ACLKCONbits.APSTSCLR = 7; // Divide Auxiliary clock by 1
  ACLKCONbits.ENAPLL = 1;   // Enable Auxiliary PLL

  // Wait for Auxiliary PLL to Lock 
  while(ACLKCONbits.APLLCK != 1); 
  //120MHz pll
}
        

/******************************************************************************/
/******************************************************************************/

void _ISR _OscillatorFail(void){
  asm("mov OSCCON, w0");      //Read OSCCON in to w0
  asm("bclr w0, #0x08");      //Before clearing the OSCFAIL trap status flag
                              //we should clear the Clock Fail(CF) bit in
                              //OSCCONL

  //Load w0 with a byte value to write to
  //OSCCONL so that the CF bit is cleared
  asm("mov #OSCCONL, w1");

  //Point w1 to OSCCONL byte
  asm("disi #6");

  //Disable interrupts for the next    
  //5 instruction cycles
  asm("mov #0x46, w2");

  //Move first OSCCONL unlock code byte to w2
  asm("mov #0x57, w3");

  //Move second OSCCONL unlock code byte to w3
  asm("mov.b w2, [w1]");

  //Perform byte-wide move of w2 to OSCCONL
  asm("mov.b w3, [w1]");

  //Perform byte-wide move of w2 to OSCCONL
  asm("mov.b w0, [w1]");        
  //Write parameter in w0 (lower byte)to
  //OSCCONL within one instruction cycle
  //of unlocking OSCCONL using MOV instruction
  // Clear the trap flag
  INTCON1bits.OSCFAIL = 0;        
  while(1);     
}       

/******************************************************************************/
/******************************************************************************/

