/*******************************************************************************
packet.c
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

#include "packet.h"
#include "can.h"
#include "leds.h"
#include "main.h"
#include "adc.h"
#include "params.pb.h"
#include "state.pb.h"
#include "cobs.h"
#include <pb.h>
#include <pb_common.h>

/******************************************************************************/
/******************************************************************************/

static uint8 ibuf[StateMessage_size];
static uint8 obuf[StateMessage_size];
static uint8 *obuf_ptr = obuf;
static uint16 pkt_nbr; 
static uint8 not_done = False;

extern StateMessage state;
extern ParamsMessage params;

/******************************************************************************/
/******************************************************************************/

// The protobuf state stream (debug_mode) is not built: it needs pb_encode,
// which no longer fits in program memory. Use the photon_service registers.
void SendStateMessageCAN(void){
}

/******************************************************************************/
/******************************************************************************/

void SendBurstMessageData(void){
  uint8 pkt[8] = {0,0,0,0,0,0,0,0};     
  uint8 c;
  uint8 i;
  c = *obuf_ptr;
  
  union u_t u;
        
  u.lu = 0;
  if(state.burst_pkts_ch == 0){
    u.su.lu = ADCBUF0;
  }else{
    u.su.lu = ADCBUF2;
  }

  pkt[0] = u.cu[0];
  pkt[1] = u.cu[1];

  u.lu = 0;
  if(state.burst_pkts_ch == 0){
    u.su.lu = ADCBUF1;
  }else{
    u.su.lu = ADCBUF3;
  }
  pkt[2] = u.cu[0];
  pkt[3] = u.cu[1];

  u.lu = 0;
  u.su.lu = state.chan[state.burst_pkts_ch].v_raw;
  pkt[4] = u.cu[0];
  pkt[5] = u.cu[1];

  u.lu = 0;
  u.su.lu = state.chan[state.burst_pkts_ch].i_raw;
  pkt[6] = u.cu[0];
  pkt[7] = u.cu[1];


  sendCANMsg(state.n_burst_pkts, pkt, 8);
  
  if(state.n_burst_pkts > 0){
    state.n_burst_pkts--;
  }
}
/******************************************************************************/
/******************************************************************************/

void SendStateMessageData(void){
}

/******************************************************************************/
/******************************************************************************/

static float v_scale = 100;
static float i_scale = 1000;

void SendMeasurementPacket(int channel){
  uint8 data[8] = {0,0,0,0,0,0,0,0};      
  int32 vin = state.chan[channel].v * v_scale;
  int32 vout = state.batt_v * v_scale;
  int32 current = state.chan[channel].i * i_scale;
  union u_t u;

  
  u.lu = 0;
  u.su.lu = vin;
  data[0] = u.cu[0];
  data[1] = u.cu[1];
  
  u.lu = 0;
  u.su.lu = current;
  data[2] = u.cu[0];
  data[3] = u.cu[1];
  
  u.lu = 0;
  u.su.lu = vout;
  data[4] = u.cu[0];
  data[5] = u.cu[1];
                  
  u.lu = 0;
  u.ss.ls = state.chan[channel].temp;
  data[6] = u.cu[0];
  data[7] = u.cu[1];
  
  sendCANMsg(params.comm.can_base_addr+channel, data, 8); 
       
}       

/******************************************************************************/
/******************************************************************************/

void sendDebugPacket(void){
  uint8 data[8] = {0,0,0,0,0,0,0,0};      
  uint16 vin = 0;
  uint16 iin = 0;
  uint16 vout = 0;
  uint16 iout = 0;
  
  union u_t u;
  
  u.lu = 0;
  u.su.lu = vin;
  data[0] = u.cu[0];
  data[1] = u.cu[1];
  
  u.lu = 0;
  u.su.lu = iin;
  data[2] = u.cu[0];
  data[3] = u.cu[1];

  u.lu = 0;
  u.su.lu = vout;
  data[4] = u.cu[0];
  data[5] = u.cu[1];
  
  u.lu = 0;
  u.su.lu = iout;
  data[6] = u.cu[0];
  data[7] = u.cu[1];
  
  sendCANMsg(0x73, data, 8);      
}

/******************************************************************************/
/******************************************************************************/        
