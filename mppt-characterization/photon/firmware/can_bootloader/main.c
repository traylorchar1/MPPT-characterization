/*******************************************************************************
main.c
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

#include "can.h"
#include "general.h"
#include "main.h"

#include "../photon_3/Photon_3_0/params.pb.h"

// Select Internal FRC at POR
_FOSCSEL(FNOSC_FRC & IESO_ON);
// Enable Clock Switching and Configure POSC in XT mode
_FOSC(FCKSM_CSECME & OSCIOFNC_OFF  & POSCMD_XT);  

//Watchdog timer
_FWDT(FWDTEN_ON & WINDIS_OFF & WDTPRE_PR32 & WDTPOST_PS8192);  
//_FWDT(FWDTEN_OFF);

//Power On Brown Out Reset Off
//Master CLR Enabled
//_FBORPOR(PBOR_OFF & MCLR_EN);

//Power On Reset Delay
//128ms delay to initialization
_FPOR(FPWRT_PWR128);

//ICD Setup
//Device will Reset in user mode
//Reset in operational mode
//JTAG is disabled
//communicate on PGC2/EMUC2 and PGD2/EMUD2
_FICD(JTAGEN_OFF & ICS_PGD2);

//_FGS(GCP_OFF & GSS_OFF & GWRP_OFF); 

/******************************************************************************/
/******************************************************************************/
//Global Declarations

volatile ParamsMessage params;

volatile int therm_t[1023];
uint8 ibuf[ParamsMessage_size];

/******************************************************************************/
/******************************************************************************/

int main(void){
  Initialize();
  while(1){
    CanPseudoInterrupt();
    CheckCountdownTimer();
    ClrWdt();
  }
}

/******************************************************************************/
/******************************************************************************/
