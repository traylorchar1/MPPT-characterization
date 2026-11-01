/*******************************************************************************
i2c.h
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

#ifndef I2C_DPS_H
#define I2C_DPS_H

#include "general.h"

/******************************************************************************/
/******************************************************************************/
/*Declarations*/

#define TMP422_WRITE (0b10011000)
#define TMP422_READ  (0b10011001)
#define TMP422_LOCAL (0x00)
#define TMP422_REM_0 (0x01)
#define TMP422_REM_1 (0x02)

#define MCP4728_WRITE (0b11000000)
#define MCP4728_CHA   (0b01000000)
#define MCP4728_CHB   (0b01000010)
#define MCP4728_CHC   (0b01000100)
#define MCP4728_CHD   (0b01000110)

#define DAC_V_REF_0   MCP4728_CHA
#define DAC_V_REF_1   MCP4728_CHB
#define DAC_I_REF_0   MCP4728_CHC
#define DAC_I_REF_1   MCP4728_CHD

#define BRG_100K  (394)
#define BRG_200K  (194)
#define BRG_400K  (94)

/******************************************************************************/
/******************************************************************************/
/*Function Definitions*/

uint16 GetTemp(uint16 reg);
void SetDacVoltageMcp4728(uint8 ch, uint16 code);
void InitI2C(uint16 speed);
void Mcp4728ControlVoltageFSM(void);

/******************************************************************************/
/******************************************************************************/
/*Macro Defintions*/

#endif

