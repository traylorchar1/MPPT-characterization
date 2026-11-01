/*******************************************************************************
   fpga.c
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
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
   THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*******************************************************************************/

#include "build.h"
#include "debug.h"
#include "fpga.h"
#include "fram.h"
#include "general.h"
#include "leds.h"
#include "timer.h"

#include "main.h"
#include <spi.h>
#include <stdio.h>
#include <string.h>

/******************************************************************************/
/******************************************************************************/

void InitFpga(void)
{
  // initialize the SPI link
  uint16 config1, config2, config3;

  // config sclk as outoput
  TRISFbits.TRISF6 = 0;

  // config MISO as input
  TRISFbits.TRISF2 = 1;

  // config MOSI as outut
  TRISFbits.TRISF3 = 0;

  // config CS as output
  TRISDbits.TRISD8 = 0;

  // configure FPGA reset as output
  TRISDbits.TRISD1 = 0;

  // configure FPGA en as output
  TRISEbits.TRISE4 = 0;

  // configure FPGA done as input
  TRISDbits.TRISD0 = 1;

  // configure FPGA power as output
  TRISDbits.TRISD11 = 0;

  // configure FPGA fault as input
  TRISDbits.TRISD10 = 1;

  // configure DSP_GPIOs as output
  TRISEbits.TRISE0 = 0;   // reset mppt 0
  TRISEbits.TRISE1 = 0;   // relay precharge
  TRISEbits.TRISE2 = 0;   // reset mppt 1
  TRISEbits.TRISE3 = 0;   // relay main


  DisableFpgaMppt0();
  DisableFpgaMppt1();

  // deselect the chip
  DeselectFpga();

  // build config bytes
  // normal SPI mode 0, 1.25MHz
  config1 =
    ENABLE_SCK_PIN &
    ENABLE_SDO_PIN &
    SPI_MODE16_OFF &
    SPI_SMP_OFF &
    SPI_CKE_OFF &
    SLAVE_ENABLE_OFF &
    CLK_POL_ACTIVE_LOW &
    MASTER_ENABLE_ON &
    SEC_PRESCAL_8_1 &
    PRI_PRESCAL_4_1;

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
  OpenSPI1(config1, config2, config3);

  // disable the interrupt
  ConfigIntSPI1(SPI_INT_DIS);
}

/******************************************************************************/
/******************************************************************************/

void ResetFpga(void)
{
  // Configuring as SPI slave
  // tCRESET_B = 200ns

  // While the rst line is low, assert CS to make the FPGA come up as SPI slave
  SetResetFpga(0); 
  
  KillFpga();
  delayMilliseconds(1);
  SelectFpga();
  PowerFpga();
  DisableFpgaMppt0();
  DisableFpgaMppt1();
  delayMilliseconds(20);

  SetResetFpga(1); 
  delayMilliseconds(1);

}

/******************************************************************************/
/******************************************************************************/

uint8 SpiRWFpga(uint8 wr)
{
  uint8 rd;

  // spin while the transmit buffer is full, basically wait for previous
  // transactions to complete
  while (SPI1STATbits.SPITBF);

  // write the byte to send out
  WriteSPI1(wr);

  // spin until the readback byte is ready
  while (!DataRdySPI1());

  // transfer the readback byte
  rd = ReadSPI1();

  return rd;
}

/******************************************************************************/
/******************************************************************************/

inline void SetResetFpga(uint8 state)
{
  // set the reset line to the specified state, 0 = reset, 1 = normal
  LATDbits.LATD1 = state;

}

/******************************************************************************/
/******************************************************************************/

inline void SetCsFpga(uint8 state)
{
  // set the chip select line to the specified state, 0 = select, 1 = deselect
  LATDbits.LATD8 = state;
}

/******************************************************************************/
/******************************************************************************/

void WriteEnableFpga(uint8 enable)
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
  SelectFpga();

  // send the opcode
  SpiRWFpga(opcode);

  // deselect the chip
  DeselectFpga();

  return;
}

/******************************************************************************/
/******************************************************************************/

uint8 ReadWriteByteFpga(uint16 addr, uint8 wr, uint8 mode)
{
  // function to write a single byte to FPGA chip

  // the opcode to send to the chip
  uint8 opcode;

  // address h and addr l -> mask to 13 bits -> 8192 addresses
  uint8 addrh = (addr >> 8) & 0x1f;
  uint8 addrl = (addr >> 0) & 0xff;

  // rd holds the byte thats just been read
  uint8 rd;

  // if we're reading from the chip, set opcode = 0x03
  if (mode == FPGA_READ) {
    opcode = 0x03;

    // if we're writing, set opcode = 0x02, also enable writing
  }else if (mode == FPGA_WRITE) {
    opcode = 0x02;
    WriteEnableFram(1);

    // else, if we've received a bad mode command - return 0
  }else{
    return 0;
  }

  // select the chip
  SelectFpga();

  // send the opcode
  SpiRWFpga(opcode);

  // send the addrh
  SpiRWFpga(addrh);

  // send the addrl
  SpiRWFpga(addrl);

  // clock out the data and read the byte
  rd = SpiRWFpga(wr);

  // deselect the chip
  DeselectFpga();

  // return the read byte
  return rd;
}

/******************************************************************************/
/******************************************************************************/

uint8 ProgramFpga(void)
{
  int32 i;
  uint8 rd;
  union u_t bitstream_length;
  
  DeselectFram();

  // Select the Flash
  SelectFlash();

  // Place the flash into fast-read mode
  SpiRW2(OP_FLASH_RD);

  // Set address zero as four 0 bytes;
  for (i=0; i<3; i++){
    SpiRW2(0);
  }

  // Load the length of the bitstream from flash
  for (i=0; i<4; i++){
    rd = SpiRW2(0);
    bitstream_length.u8[i] = rd;
  }

  // Place the FPGA into prgoramming mode
  ResetFpga();
  DisableFpga();

  // Read the first byte from flash
  rd = SpiRW2(0);

  // Send the dummy byte to the fpga
  WriteSPI1(0);

  // Send the bitstream a byte at a time
  for (i=0; i<bitstream_length.u32; i++){
    // Read the next byte from flash
    while (SPI2STATbits.SPITBF);
    WriteSPI2(0);

    // Write the last byte to the FPGA
    while (SPI1STATbits.SPITBF);
    WriteSPI1(rd);

    // Read out the next byte from the Flash
    while (!DataRdySPI2());
    rd = ReadSPI2();

  } 

  // Send ~100 extra clocks
  for (i=0; i<100; i++){
    while (SPI1STATbits.SPITBF);
    WriteSPI1(0);
  }

  // Deselect the Flash
  DeselectFlash();
  
  // Deselect the FPGA
  DeselectFpga();
  
  EnableFpga();

  return PORTDbits.RD0;
}

/******************************************************************************/
/******************************************************************************/

