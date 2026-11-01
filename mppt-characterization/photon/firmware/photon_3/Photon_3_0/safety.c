/*******************************************************************************
safety.c
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

#include "safety.h"
#include "main.h"
#include "adc.h"
#include "pwm.h"
#include "comparator.h"
#include "state.pb.h"
#include "params.pb.h"
#include "photon_service.h"

/******************************************************************************/
/******************************************************************************/

extern volatile StateMessage state;
extern volatile ParamsMessage params;

/******************************************************************************/
/******************************************************************************/

int32 max_temp;
int32 max_voltage;
int32 max_current_batt;
int32 max_current_arr;

/******************************************************************************/
/******************************************************************************/

ControlEnum ErrorStateOpen(void){
    return ControlEnum_ERROR_OPEN_RELAY;
}

/******************************************************************************/
/******************************************************************************/

ControlEnum ErrorStateClosed(ControlEnum ctrl){
  if(state.control == ControlEnum_TRACKING){
    return ControlEnum_ERROR_CLOSED_RELAY;
  }else if(state.control == ControlEnum_ERROR_CLOSED_RELAY){
    return ControlEnum_ERROR_CLOSED_RELAY;
  }else{
    return ControlEnum_ERROR_OPEN_RELAY;
  }
}

/******************************************************************************/
/******************************************************************************/

void CheckSafety(void){
  // Check for overvoltage conditions
  if(CheckOVComparator()){
    disablePWM();
    state.safety = SafetyEnum_COMP_OV;
    state.control = ErrorStateOpen(); 
  }else if(state.batt_v_raw > max_voltage){
    state.safety = SafetyEnum_BATT_OV;
    state.control = ErrorStateOpen(); 
  }else if(state.chan[0].v_raw > max_voltage){
    state.safety = SafetyEnum_ARR_0_OV; 
    state.control = ErrorStateOpen(); 
  }else if(state.chan[1].v_raw > max_voltage){
    state.safety = SafetyEnum_ARR_1_OV; 
    state.control = ErrorStateOpen(); 
 
  // Check for overcurrent conditions
  }else if(state.batt_i_raw > max_current_batt){
    state.safety = SafetyEnum_BATT_OC;
    state.control = ErrorStateClosed(state.control); 
  }else if(state.chan[0].i_raw > (max_current_arr = photon_tuning.oc_arr_raw)){
    state.safety = SafetyEnum_ARR_0_OC;
    state.control = ErrorStateClosed(state.control); 
  }else if(state.chan[1].i_raw > max_current_arr){
    state.safety = SafetyEnum_ARR_1_OC;
    state.control = ErrorStateClosed(state.control); 

  // Check for over temp conditions
  }else if(state.temp_ambient > max_temp){
    state.safety = SafetyEnum_AMB_OT;
    state.control = ErrorStateClosed(state.control); 
  
  }else if(state.chan[0].temp > max_temp){
    state.safety = SafetyEnum_ARR_0_OT;
    state.control = ErrorStateClosed(state.control); 
  
  }else if(state.chan[1].temp > max_temp){
    state.safety = SafetyEnum_ARR_1_OT;
    state.control = ErrorStateClosed(state.control); 

  }else{
    state.safety = SafetyEnum_NO_ERROR;
    if(state.control == ControlEnum_ERROR_OPEN_RELAY){
      state.control = ControlEnum_INIT;
    }else if(state.control == ControlEnum_ERROR_CLOSED_RELAY){
      state.control = ControlEnum_STARTING;
    } 
  }
}

/******************************************************************************/
/******************************************************************************/

int16 CheckOVComparator(void){
  if(params.device.hw_rev >= PHOTON_3_2){
    return False;
  }else{
    return !PORTDbits.RD8;
  }

  
}

/******************************************************************************/
/******************************************************************************/

void InitSafety(void){
  // Enable input  
  TRISDbits.TRISD8 = 1;

  max_voltage = 160 * 6.13 * ADC_FILT_LEN;   // 160V
  max_current_arr = 8 * 128 * ADC_FILT_LEN;  // 7.5A
  max_current_batt = 15 * 64 * ADC_FILT_LEN; // 15A
  max_temp = 9500;
}

/******************************************************************************/
/******************************************************************************/
