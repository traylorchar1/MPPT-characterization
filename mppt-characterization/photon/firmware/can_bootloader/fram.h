/*******************************************************************************
fram.h
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

#ifndef FRAM_H
#define FRAM_H

#include "general.h"

/******************************************************************************/
/******************************************************************************/
// Constants

#define FRAM_READ 0
#define FRAM_WRITE 1
#define FRAM_SIZE 0x2000

#define FLASH_READ 0x10
#define FLASH_WRITE 0x11

#define OP_FLASH_PP 0x02
#define OP_FLASH_RD 0x03
#define OP_FLASH_WRITE_DISABLE 0x04
#define OP_FLASH_READ_STATUS 0x05
#define OP_FLASH_WRITE_ENABLE 0x06
#define OP_FLASH_FAST_READ 0x0B
#define OP_FLASH_CHIP_ERASE 0xC7

#define FLASH_OFFSET 0x0

/******************************************************************************/
/******************************************************************************/
// Functions

void InitFram(void);
uint8 SpiRW2(uint8 data);
uint8 ReadWriteByteFram(uint16 addr, uint8 wr, uint8 mode);
void WriteBytesFlash(uint32 addr, uint8 *wr, uint16 len);
void ReadBytesFlash(uint32 addr, uint8 *rd, uint16 len);
inline void SetCsFram(uint8 state);
inline void SetCsFlash(uint8 state);
void WriteEnableFram(uint8 enable);
void EraseFlash(void);
uint8 ReadFlashStatus(void);
void FastWriteSPI2(uint8 wr);
uint8 FastReadSPI2(void);

/******************************************************************************/
/******************************************************************************/
// Macros

#define SelectFlash()   LATEbits.LATE5 = 0
#define DeselectFlash() LATEbits.LATE5 = 1
#define SelectFram()    LATGbits.LATG9 = 0
#define DeselectFram()  LATGbits.LATG9 = 1


/******************************************************************************/
/******************************************************************************/

#endif
