/*******************************************************************************
fpga.h
Copyright (c) 2017, Samuel Lenius
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

#ifndef FPGA_H
#define FPGA_H

#include "general.h"

/******************************************************************************/
/******************************************************************************/

#define FPGA_READ 0
#define FPGA_WRITE 1
#define FPGA_SIZE 0x20000


void InitFpga(void);
uint8 SpiRWFpga(uint8 data);
uint8 ReadWriteByteFpga(uint16 addr, uint8 wr, uint8 mode);
inline void SetCsFpga(uint8 state);
inline void SetResetFpga(uint8 state);
void ResetFpga(void);
void WriteEnableFpga(uint8 enable);
uint8 ProgramFpga(void);

#define SelectFpga()    (LATDbits.LATD8 = 0)
#define DeselectFpga()  (LATDbits.LATD8 = 1)
#define PowerFpga()     (LATDbits.LATD11 = 1)
#define KillFpga()      (LATDbits.LATD11 = 0)

#define EnableFpga()    (LATEbits.LATE4 = 0)
#define DisableFpga()   (LATEbits.LATE4 = 1)

#define EnableFpgaMppt0() (LATEbits.LATE0 = 0)
#define DisableFpgaMppt0() (LATEbits.LATE0 = 1)
#define GetFpgaMpptEnableState0() (!LATEbits.LATE0)

#define EnableFpgaMppt1() (LATEbits.LATE2 = 0)
#define DisableFpgaMppt1() (LATEbits.LATE2 = 1)
#define GetFpgaMpptEnableState1() (!LATEbits.LATE2)


/******************************************************************************/
/******************************************************************************/

#endif
