/*******************************************************************************
   fram.c
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

#include "build.h"
#include "debug.h"
#include "fram.h"
#include "general.h"
#include "leds.h"
#include "timer.h"

#include "main.h"
#include <spi.h>
#include <string.h>
#include <stdio.h>

/******************************************************************************/
/******************************************************************************/

void InitFram(void)
{
  // initialize the FRAM chip
  uint16 config1, config2, config3;

  // config sclk as outoput
  TRISGbits.TRISG6 = 0;

  // config MISO as input
  TRISGbits.TRISG7 = 1;

  // config MOSI as outut
  TRISGbits.TRISG8 = 0;

  // config CS FRAM as output
  TRISGbits.TRISG9 = 0;

  // config CS FLASH as output
  TRISEbits.TRISE5 = 0;

  // deselect the chips
  DeselectFram();
  DeselectFlash();

  // build config bytes
  // normal SPI mode 0, 5MHz
  config1 =
    ENABLE_SCK_PIN &
    ENABLE_SDO_PIN &
    SPI_MODE16_OFF &
    SPI_SMP_OFF &
    SPI_CKE_ON &
    SLAVE_ENABLE_OFF &
    CLK_POL_ACTIVE_HIGH &
    MASTER_ENABLE_ON &
    SEC_PRESCAL_8_1 &
    PRI_PRESCAL_1_1;

  config2 =
    FRAME_ENABLE_OFF &
    FRAME_SYNC_OUTPUT &
    FRAME_POL_ACTIVE_HIGH &
    FRAME_SYNC_EDGE_PRECEDE &
    FIFO_BUFFER_DISABLE;

  config3 =
    SPI_ENABLE &
    SPI_IDLE_CON &
    SPI_RX_OVFLOW_CLR;

  // send the config bytes to the module
  OpenSPI2(config1, config2, config3);

  // disable the interrupt
  ConfigIntSPI2(SPI_INT_DIS);
}

/******************************************************************************/
/******************************************************************************/

uint8 SpiRW2(uint8 wr)
{
  // spin while the transmit buffer is full, basically wait for previous
  // transactions to complete
  while (SPI2STATbits.SPITBF);

  // Dummy read out old data
  if (SPI2STATbits.SPIRBF)
  {
    ReadSPI2();
  }
  

  // write the byte to send out
  WriteSPI2(wr);

  // spin until the readback byte is ready
  while (!DataRdySPI2());

  // transfer the readback byte
  return ReadSPI2();
}

/******************************************************************************/
/******************************************************************************/

void FastWriteSPI2(uint8 wr)
{
  while (SPI2STATbits.SPITBF);
  WriteSPI2(wr);
  while (SPI2STATbits.SPITBF);
}

/******************************************************************************/
/******************************************************************************/

uint8 FastReadSPI2(void)
{
  return 0;
}

/******************************************************************************/
/******************************************************************************/
// Macro'd
//inline void SetCsFlash(uint8 state)
//{
  // set the chip select line to the specified state, 0 = select, 1 = deselect
//  LATEbits.LATE5 = state;
//}

/******************************************************************************/
/******************************************************************************/
// Macro'd
//inline void SetCsFram(uint8 state)
//{
  // set the chip select line to the specified state, 0 = select, 1 = deselect
//  LATGbits.LATG9 = state;
//}

/******************************************************************************/
/******************************************************************************/

void WriteEnableFram(uint8 enable)
{
  // opcode holds the operation we want the chip to perform
  uint8 opcode;

  // if we want to enable writing, send 0x06
  if (enable) {
    opcode = 0x06;

    // otherwise send 0x04 to disable writing
  }else
    opcode = 0x04;

  // select the chip
  SelectFram();

  // send the opcode
  SpiRW2(opcode);

  // deselect the chip
  DeselectFram();

  return;
}

/******************************************************************************/
/******************************************************************************/

void WriteEnableFlash(uint8 enable)
{
  uint8 status;

  // select the chip
  SelectFlash();

  // if we want to enable writing, send 0x06
  if (enable) {
    SpiRW2(OP_FLASH_WRITE_ENABLE);
  }else{
    SpiRW2(OP_FLASH_WRITE_DISABLE);
  }
  // deselect the chip
  DeselectFlash();

  // Spin until the write protest bit is either set or unset
  if (enable) {
    do {
      status = ReadFlashStatus();
    } while(status!= 0x02);
  }else{
    do {
      status = ReadFlashStatus();
    } while(status!= 0x00);
  }


  return;
}

/******************************************************************************/
/******************************************************************************/

uint8 ReadWriteByteFram(uint16 addr, uint8 wr, uint8 mode)
{
  // function to write a single byte to FRAM chip

  // the opcode to send to the chip
  uint8 opcode;

  // address h and addr l -> mask to 13 bits -> 8192 addresses
  uint8 addrh = (addr >> 8) & 0x1f;
  uint8 addrl = (addr >> 0) & 0xff;

  // rd holds the byte thats just been read
  uint8 rd;

  // if we're reading from the chip, set opcode = 0x03
  if (mode == FRAM_READ) {
    opcode = 0x03;

    // if we're writing, set opcode = 0x02, also enable writing
  }else if (mode == FRAM_WRITE) {
    opcode = 0x02;
    WriteEnableFram(1);

    // else, if we've received a bad mode command - return 0
  }else{
    return 0;
  }

  // select the chip
  SelectFram();

  // send the opcode
  SpiRW2(opcode);

  // send the addrh
  SpiRW2(addrh);

  // send the addrl
  SpiRW2(addrl);

  // clock out the data and read the byte
  rd = SpiRW2(wr);

  // deselect the chip
  DeselectFram();

  // return the read byte
  return rd;
}

/******************************************************************************/
/******************************************************************************/

void WriteBytesFlash(uint32 addr, uint8 *wr, uint16 len)
{
  uint8 addr2;
  uint8 addr1;
  uint8 addr0;
  uint16 i;

  addr = addr + FLASH_OFFSET;
  addr2 = (addr >> 16) & 0xff;
  addr1 = (addr >> 8) & 0xff;
  addr0 = (addr >> 0) & 0xff;
  
  WriteEnableFlash(1);

  // select the chip
  SelectFlash();

  // send the opcode
  SpiRW2(OP_FLASH_PP);

  // send the addr
  SpiRW2(addr2);
  SpiRW2(addr1);
  SpiRW2(addr0);

  // clock out the data
  for(i=0; i<len; i++){
    SpiRW2(wr[i]);
  }

  // deselect the chip
  DeselectFlash();

  delay10Microseconds();
  delay10Microseconds();
  delay10Microseconds();

}

/******************************************************************************/
/******************************************************************************/

void ReadBytesFlash(uint32 addr, uint8 *rd, uint16 len)
{
  uint8 addr2;
  uint8 addr1;
  uint8 addr0;
  uint16 i;

  addr = addr + FLASH_OFFSET;
  addr2 = (addr >> 16) & 0xff;
  addr1 = (addr >> 8) & 0xff;
  addr0 = (addr >> 0) & 0xff;

  // select the chip
  SelectFlash();

  // send the opcode
  SpiRW2(OP_FLASH_RD);

  // send the addr
  SpiRW2(addr2);
  SpiRW2(addr1);
  SpiRW2(addr0);

  // clock in the data
  for(i=0; i<len; i++){
    rd[i] = SpiRW2(0);
  }

  // deselect the chip
  DeselectFlash();
}

/******************************************************************************/
/******************************************************************************/

uint8 ReadFlashStatus(void)
{
  uint8 status;
  
  SelectFlash();
  SpiRW2(OP_FLASH_READ_STATUS);
  status = SpiRW2(0);
  DeselectFlash();

  return status;
}

/******************************************************************************/
/******************************************************************************/

void EraseFlash(void)
{
  ReadFlashStatus();
  
  WriteEnableFlash(1);

  ReadFlashStatus();
  
  SelectFlash();
  SpiRW2(OP_FLASH_CHIP_ERASE);
  DeselectFlash();
  
  ReadFlashStatus();

  WriteEnableFlash(0);
  
  ReadFlashStatus();



}

/******************************************************************************/
/******************************************************************************/


