/*******************************************************************************
   timer.c
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

#include "timer.h"
#include "main.h"
#include "leds.h"
#include "oscillator.h"
#include "debug.h"

/******************************************************************************/
/******************************************************************************/
/*
void _ISR _T1Interrupt(void)
{
	IFS0bits.T1IF = 0; //Clear Timer1 interrupt flag
	return;
}
*/
/******************************************************************************/
/******************************************************************************/
/*
void _ISR _T2Interrupt(void)
{

	IFS0bits.T2IF = 0; //Clear Timer2 interrupt flag
	return;
}
*/
/******************************************************************************/
/******************************************************************************/
/*
void _ISR _T3Interrupt(void)
{
	IFS0bits.T3IF = 0; //Clear Timer3 interrupt flag
	return;
}
*/
/******************************************************************************/
/******************************************************************************/
/*
void _ISR _T4Interrupt(void)
{
	IFS1bits.T4IF = 0; //Clear Timer4 interrupt flag
	return;
}
*/
/******************************************************************************/
/******************************************************************************/
/*
void _ISR _T5Interrupt(void)
{
	IFS1bits.T5IF = 0; //Clear Timer5 interrupt flag
	return;
}
*/
/******************************************************************************/
/******************************************************************************/

void delayMilliseconds(int milliseconds)
{

	while (milliseconds-- > 0)
		delay1Millisecond();

	return;
}

/******************************************************************************/
/******************************************************************************/

void delay1Millisecond(void)
{

	IFS1bits.T4IF = 0;
	TMR4 = 0;
	PR4 = 39986;
	T4CONbits.TON = 1;
	while (IFS1bits.T4IF == 0) ;
	IFS1bits.T4IF = 0;
	T4CONbits.TON = 0;

	return;
}

/******************************************************************************/
/******************************************************************************/

void delay100Microseconds(void)
{

	TMR4 = 0;
	T4CONbits.TON = 1;
	PR4 = 3986;
	IFS1bits.T4IF = 0;
	while (IFS1bits.T4IF == 0) ;
	IFS1bits.T4IF = 0;
	T4CONbits.TON = 0;

	return;
}

/******************************************************************************/
/******************************************************************************/

void delay10Microseconds(void)
{

	TMR4 = 0;
	T4CONbits.TON = 1;
	PR4 = 386;
	IFS1bits.T4IF = 0;
	while (IFS1bits.T4IF == 0) ;
	IFS1bits.T4IF = 0;
	T4CONbits.TON = 0;

	return;
}

/******************************************************************************/
/******************************************************************************/

void delay1Microsecond(void)
{

	TMR4 = 0;
	T4CONbits.TON = 1;
	PR4 = 25;
	IFS1bits.T4IF = 0;
	while (IFS1bits.T4IF == 0) ;
	IFS1bits.T4IF = 0;
	T4CONbits.TON = 0;

	return;
}

/******************************************************************************/
/******************************************************************************/

void delayMicroseconds(uint16 microseconds)
{

	while (microseconds >= 1000) {
		delay1Millisecond();
		microseconds -= 1000;
	}

	if (microseconds == 0)  return;
	TMR4 = 0;
	T4CONbits.TON = 1;
	PR4 = (40 * microseconds) - 16;
	IFS1bits.T4IF = 0;
	while (IFS1bits.T4IF == 0) ;
	IFS1bits.T4IF = 0;
	T4CONbits.TON = 0;

	return;
}

/******************************************************************************/
/******************************************************************************/

void InitTimer(uint8 channel, uint16 period, uint16 option1, uint8 option2)
{

	switch (channel) {
	case 1:
		IPC0bits.T1IP = option2 & 0b111;
		PR1 = period;
		TMR1 = 0;
		T1CON = option1;
		IEC0bits.T1IE = (option2 >> 7) & 0b1;
		IFS0bits.T1IF = 0;
		break;

	case 2:
		IPC1bits.T2IP = option2 & 0b111;
		PR2 = period;
		TMR2 = 0;
		T2CON = option1;
		IEC0bits.T2IE = (option2 >> 7) & 0b1;
		IFS0bits.T2IF = 0;
		break;

	case 3:
		IPC2bits.T3IP = option2 & 0b111;
		PR3 = period;
		TMR3 = 0;
		T3CON = option1;
		IEC0bits.T3IE = (option2 >> 7) & 0b1;
		IFS0bits.T3IF = 0;
		break;

	case 4:
		IPC6bits.T4IP = option2 & 0b111;
		PR4 = period;
		TMR4 = 0;
		T4CON = option1;
		IEC1bits.T4IE = (option2 >> 7) & 0b1;
		IFS1bits.T4IF = 0;
		break;

	case 5:
		IPC7bits.T5IP = option2 & 0b111;
		PR5 = period;
		TMR5 = 0;
		T5CON = option1;
		IEC1bits.T5IE = (option2 >> 7) & 0b1;
		IFS1bits.T5IF = 0;
		break;

	default:        break;
	}
	return;
}

/******************************************************************************/
/******************************************************************************/

