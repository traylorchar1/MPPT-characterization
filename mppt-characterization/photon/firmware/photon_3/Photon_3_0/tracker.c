/*******************************************************************************
tracker.c
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

#include "tracker.h"

#include "adc.h"
#include "can.h"
#include "float.h"
#include "fpga.h"
#include "i2c.h"
#include "leds.h"
#include "main.h"
#include <math.h>
#include "params.pb.h"
#include "pwm.h"
#include "safety.h"
#include "state.pb.h"
#include "photon_service.h"

/******************************************************************************/
/******************************************************************************/
/*Private Macros*/

#define MINVOLT photon_tuning.min_voltage_raw
#define MAXVOLT photon_tuning.max_voltage_raw
#define MINDAC (8 * ADC_FILT_LEN)   // About 0.66A * 2
#define STABILITY_MARGIN (4)  // One count is ~20 mV, so this is about 80 mV.

#define DAC_SHIFT 1

#define SWEEP_STEP_SIZE (8 * ADC_FILT_LEN)
#define SWEEP_DAC_STEP_SIZE photon_tuning.sweep_dac_step


static uint16 MAXDAC = 0;
static int32 voltage_hyst[2];

// Power P&O state, exposed through photon_service for diagnostics.
int32 po_step[2], po_p_prev[2], po_v_avg[2], po_i_avg[2];
int16 po_clamp[2];
static int16 po_tick[2], po_run[2], po_have_prev[2], po_near[2];
// Accumulated by ControlVoltages (200 Hz) during the averaging window.
static int32 po_acc_v[2], po_acc_i[2], po_acc_n[2];
static bool po_accumulate[2];
// Fractional part of the PI integrator, 1/256 DAC counts.
static int16 i_frac[2];
// Current ceiling learned from source collapses (DAC counts); creeps back up.
static int32 soft_max[2] = {4095, 4095};
static int16 since_collapse[2];

static void PoReset(uint8 c){
  po_tick[c] = 0;
  po_run[c] = 0;
  po_have_prev[c] = 0;
  po_step[c] = photon_tuning.po_step_min;
  po_accumulate[c] = false;
  po_clamp[c] = 0;
  po_near[c] = 0;
  i_frac[c] = 0;
}

/******************************************************************************/
/******************************************************************************/
/*Declarations*/


extern volatile StateMessage state;
extern volatile ParamsMessage params;
extern volatile int thermistor_table[1023];

/******************************************************************************/
/******************************************************************************/

void InitTracker(void){
  uint16 c;
  
  if(params.device.hw_rev >= PHOTON_3_2){
    // Rev 3.2+ has a 12 bit dac
    // https://www.wolframalpha.com/input/?i=16amps+*+2milliohms+*+4990%2F100+*+4096%2F2.048volts
    MAXDAC = photon_tuning.max_dac;  // default 2023, ~7.5 amp limit 
  }else{
    MAXDAC = (130 * ADC_FILT_LEN) << DAC_SHIFT; //~7.5A limit
  }


  for(c=0; c<2; c++){
    voltage_hyst[c] = params.track.voltage_hyst / params.chan[c].v_scale;
  }


  state.max_dac = MAXDAC;
  soft_max[0] = soft_max[1] = MAXDAC;
  state.control = ControlEnum_INIT;
}

/******************************************************************************/
/******************************************************************************/

// Apply a new input current ceiling without restarting the converter.
void TrackerApplyMaxDac(void){
  if(params.device.hw_rev >= PHOTON_3_2){
    MAXDAC = photon_tuning.max_dac;
    state.max_dac = MAXDAC;
  }
  soft_max[0] = soft_max[1] = state.max_dac;
}

/******************************************************************************/
/******************************************************************************/

int32 SwitchDirection(int32 dir){
  if(dir == TrackingDirectionEnum_UP){
    return(TrackingDirectionEnum_DOWN);
  }else{
    return(TrackingDirectionEnum_UP);
  }
}

/******************************************************************************/
/******************************************************************************/


void TrackerStateMachine(void){
  uint16 c = 0;
  static uint16 run_alg = 0;
  static uint16 run_alg_loops = 0;
  static int16 test_param = 0;
  static int16 test_param_step = 0;

  DEBUG_2_TOG();
  CheckSafety();
  if (photon_disabled) state.control = ControlEnum_CAN_DISABLE;
  UpdateFloatParams();
  
  if(params.device.hw_rev >= PHOTON_3_2){
    state.fpga_fault = LATDbits.LATD10;
  }

  // Calculate the power in on each channel.
  state.batt_p_raw = state.batt_v_raw * state.batt_i_raw;
  for(c=0; c<2; c++){
    state.chan[c].p_raw = state.chan[c].v_raw * state.chan[c].i_raw;
  }

  // Decide wether or not the algorighm should run this loop
  if(++run_alg_loops >= photon_tuning.algorithm_ticks){
    run_alg = TRUE;
    run_alg_loops = 0;
  }else{
    run_alg = FALSE;
  }
    

  switch(state.control){

    case ControlEnum_INIT:
      disablePWM();
      SetPrechargeRelayState(OFF);
      SetMainRelayState(OFF);
      if(state.loops > 1){
        state.loops = 0;
        state.precharge_loops = 0;
        state.precharge_adc = 0;
        state.control = ControlEnum_PRECHARGING;
      }
      break;

    case ControlEnum_PRECHARGING:
      disablePWM();
      SetPrechargeRelayState(ON);
      SetMainRelayState(OFF);
      LED_R_ON();

      // R = 1000 ohms, C = 387 uF; R*C = 387 milliseconds
      // Enforce a minimum of ~3 RC time constants in the precharging state.
      // This comes out to about 1.2 seconds. Minimum overall precharge time
      // is thus approximately 1.7 seconds.
      if((state.loops > 60) && (state.batt_v_raw >= MINVOLT)){
        // Must spend 25 loops (~0.5 second) with better than STABILITY_MARGIN
        // output voltage stability, and above the two input channel voltages,
        // in order to exit precharge and close the main contactor.
        if(labs(state.precharge_adc - state.batt_v_raw) <= STABILITY_MARGIN){
          if((state.chan[0].v_raw <= (state.batt_v_raw + (2*ADC_FILT_LEN))) &&
              state.chan[1].v_raw <= (state.batt_v_raw + (2*ADC_FILT_LEN))){
            if(state.precharge_loops++ > 25){
              state.loops = 0;
              state.control = ControlEnum_CLOSING_MAIN;
            }
          }
        } else {
          // Reset the counter if we fail the stability test.
          state.precharge_loops = 0;
        }
      }
      state.precharge_adc = state.batt_v_raw;
      break;
    
    case ControlEnum_CLOSING_MAIN:
      disablePWM();
      SetMainRelayState(ON);
      if(state.loops > 5){
        SetPrechargeRelayState(OFF);
        state.loops = 0;
        state.control = ControlEnum_STARTING;
        LED_R_OFF();
      }
      break;

    case ControlEnum_STARTING:
      SetDacValues(MINDAC, MINDAC); 
      for(c=0; c<2; c++){
        state.chan[c].v_goal_raw = state.chan[c].v_raw;
        state.chan[c].dir = TrackingDirectionEnum_DOWN;
      }
      
      state.control = ControlEnum_TRACKING;
      //test_param = 200;
      //state.control = ControlEnum_TEST_DAC_STEP;
      state.alg = AlgorithmEnum_MEASURE_VOC;
      state.loops = 0;
      break;

    case ControlEnum_TRACKING:
      // TODO HAX
      //if(GetFpgaMpptEnableState0()){
      //  ChannelOff(0);
      //  nop(); nop(); nop(); nop();
      //  ChannelOn(0);
      //}
      //if(GetFpgaMpptEnableState1()){
      //  ChannelOff(1);
      //  nop(); nop(); nop(); nop();
      //  ChannelOn(1);
      //}

      // Switch to constant voltage mode if we're above the CV threshold
      if(state.batt_v > params.device.v_cv_charge){
        state.control = ControlEnum_CONSTANT_VOLTAGE;
      }

      switch(state.alg){
        case AlgorithmEnum_MEASURE_VOC:
          LED_G_ON();
          for(c=0; c<2; c++){
            ChannelOff(c);
            state.chan[c].v_goal_raw = MAXVOLT;
          }
          if(state.loops >= photon_tuning.voc_ticks){
            PreloadForSweep(0);
            PreloadForSweep(1);
            state.alg = AlgorithmEnum_SWEEP;
            state.loops = 0;
          }
          break;

        case AlgorithmEnum_SWEEP:
          LED_G_ON();
          LED_R_ON();
          if(run_alg){
            SweepArray(0);
            SweepArray(1);
          }
          if((state.chan[0].sweep_done && state.chan[1].sweep_done) ||
             (state.loops > photon_tuning.sweep_timeout_ticks)){ // 5 seconds max
            state.alg = photon_tuning.algorithm;
            state.loops = 0;
            PoReset(0);
            PoReset(1);
          }
          break;

        case AlgorithmEnum_PERTURB_OBSERVE:
          LED_R_OFF();
          if(photon_tuning.hold_v_raw){
            // Hold a commanded input voltage; no tracking and no retrack.
            for(c=0; c<2; c++){
              state.chan[c].v_goal_raw = MAX(MINVOLT, MIN(MAXVOLT, photon_tuning.hold_v_raw));
              po_have_prev[c] = 0;
            }
            state.loops = 0;
            break;
          }
          PerturbObserve(0);
          PerturbObserve(1);
          if(state.loops > photon_tuning.retrack_ticks){
            state.alg = AlgorithmEnum_MEASURE_VOC;
            //state.alg = photon_tuning.algorithm;
            //state.alg = AlgorithmEnum_INCREMENTAL_CONDUCTANCE;
            state.loops = 0;
          }
          break;

        case AlgorithmEnum_INCREMENTAL_CONDUCTANCE:
          if(run_alg){
            for(c=0; c<2; c++){
              state.chan[c].v_delta = state.chan[c].v - state.chan[c].v_last;
              state.chan[c].i_delta = state.chan[c].i - state.chan[c].i_last;
              state.chan[c].iv = state.chan[c].v != 0 ? state.chan[c].i / state.chan[c].v : 0;
              state.chan[c].didv = state.chan[c].v_delta != 0 ?
                  state.chan[c].i_delta / state.chan[c].v_delta : 0;
              IncrementalConductance(c);
              state.chan[c].v_last = state.chan[c].v;
              state.chan[c].i_last = state.chan[c].i;
            }
          }
          if(state.loops > photon_tuning.retrack_ticks){
            state.alg = AlgorithmEnum_MEASURE_VOC;
            state.loops = 0;
          }
          break;

      }break;

    case ControlEnum_CONSTANT_VOLTAGE:
      if(state.batt_v <
          (params.device.v_cv_charge - params.device.v_cv_charge_hyst)){
        state.control = ControlEnum_TRACKING;
      }
         
      if(state.batt_v > params.device.v_cv_charge){
        for(c=0; c<2; c++){
          state.chan[c].dac = LimitDac(state.chan[c].dac - 1);
          if(state.chan[c].dac == MINDAC){
            ChannelOff(c);
          }
        }
      }
      break;

    case ControlEnum_ERROR_OPEN_RELAY:
      disablePWM();
      LED_R_ON();
      LED_G_OFF();
      SetMainRelayState(OFF);
      SetPrechargeRelayState(OFF);
      break;
  
    case ControlEnum_ERROR_CLOSED_RELAY:
      disablePWM();
      LED_R_ON();
      LED_G_OFF();
      break;
  
    case ControlEnum_CAN_DISABLE:
      LED_G_OFF();
      LED_R_OFF();
      disablePWM();
      SetPrechargeRelayState(OFF);
      SetMainRelayState(OFF);
      state.loops = 0;
      break;
  
    case ControlEnum_TEST_DAC_STEP:
      LED_G_TOG();
      LED_R_TOG();
      ChannelOn(0);
      ChannelOn(1);
      if(state.loops < 2*50){
        state.chan[0].dac = 100;
        state.chan[1].dac = 100;
      }else if(state.loops < 4*50){
        state.chan[0].dac = test_param;
        state.chan[1].dac = test_param;
      }else{
        state.loops = 0;
        test_param += 100;
        if(test_param > MAXDAC){
          test_param = MAXVOLT;
          test_param_step = -600;
          state.control = ControlEnum_TEST_CONTROL_VOLTAGE;
        }
      }
      SetDacValues(state.chan[0].dac, state.chan[1].dac); 
      break;
  
    case ControlEnum_TEST_CONTROL_VOLTAGE:
      LED_G_TOG();
      LED_R_TOG();
      ChannelOn(0);
      ChannelOn(1);
      if(state.loops < 2*50){
        state.chan[0].v_goal_raw = test_param;
        state.chan[1].v_goal_raw = test_param;
      }else{
        state.loops = 0;
        test_param += test_param_step;
        if(test_param < MINVOLT || test_param > MAXVOLT){
          state.control = ControlEnum_TRACKING;
        }
      }
      break;
  
    case ControlEnum_TEST_MODE:
      LED_G_TOG();
      LED_R_TOG();
      SetPrechargeRelayState(ON);
      SetMainRelayState(ON);
      ChannelOn(0);
      ChannelOn(1);
      SetDacValues(state.chan[0].dac, state.chan[1].dac); 
      break;
  
    default:
      disablePWM();
      state.loops = 0;
      state.control = ControlEnum_INIT;
      break;
  }

  // Update state variables
  if(run_alg){
    state.batt_p_last_raw = state.batt_p_raw;
    state.chan[0].p_last_raw = state.chan[0].p_raw;
    state.chan[1].p_last_raw = state.chan[1].p_raw;
  }
  
  if(params.device.hw_rev == PHOTON_3_0){
      // The remote sensors are garbage on this rev - just use the processor
      // temp sensor instead.
      state.temp_ambient = GetTemp(TMP422_LOCAL);
      state.chan[0].temp = state.temp_ambient;
      state.chan[1].temp = state.temp_ambient;
  }

  state.loops++;
}

/******************************************************************************/
/******************************************************************************/
void UpdateFloatParams(void){
  int c;
  state.batt_v = state.batt_v_raw * params.device.v_scale;  
  state.batt_i = state.batt_i_raw * params.device.i_scale;  
  for(c=0; c<2; c++){
    state.chan[c].v = state.chan[c].v_raw * params.chan[c].v_scale;  
    state.chan[c].i = state.chan[c].i_raw * params.chan[c].i_scale;
  }  
}

/******************************************************************************/
/******************************************************************************/
void UpdateCvDac(void){
  if(state.batt_v < 
     (params.device.v_cv_charge - params.device.v_cv_charge_hyst)){
    if(state.max_dac < MAXDAC){
      state.max_dac++;
    }
  }
  if(state.batt_v > params.device.v_cv_charge){
    if(state.max_dac > 0){
      state.max_dac--;
    }
    else{
      disablePWM();
    }
  }
}

/******************************************************************************/
/******************************************************************************/

void UpdateVoc(uint8 c){
  if(state.chan[c].v_raw > state.chan[c].voc_raw){
    state.chan[c].voc_raw = state.chan[c].v_raw;
    state.chan[c].vsc_raw = (state.chan[c].v_raw * photon_tuning.sweep_floor_percent) / 100;
  }
}

/******************************************************************************/
/******************************************************************************/

void MaybeInitiateRetrack(){
  if(state.loops > params.track.min_retrack_time){
    state.alg = AlgorithmEnum_MEASURE_VOC;
    state.loops = 0;
  }
}


/******************************************************************************/
/******************************************************************************/

void PreloadForSweep(uint8 c){
  int new_voc = (state.chan[c].v_raw*11)/10;

  if(new_voc > state.chan[c].voc_raw){
    state.chan[c].voc_raw = new_voc;
  }
  state.chan[c].vsc_raw = (state.chan[c].v_raw * photon_tuning.sweep_floor_percent) / 100;
  state.chan[c].v_goal_raw = state.chan[c].v_raw;
  state.chan[c].v_raw_sweep_done = (state.chan[c].v_raw * photon_tuning.sweep_floor_percent) / 100;
  state.chan[c].p_raw_sweep_mpp = 0;
  state.chan[c].v_raw_sweep_mpp = 0;
  state.chan[c].dac = MINDAC;
  state.chan[c].sweep_done = False;
}

/******************************************************************************/
/******************************************************************************/

void SweepArray(uint8 c){
  if(state.chan[c].sweep_done == False){
    if(state.chan[c].p_raw > state.chan[c].p_raw_sweep_mpp){
      state.chan[c].p_raw_sweep_mpp = state.chan[c].p_raw;
      state.chan[c].v_raw_sweep_mpp = state.chan[c].v_raw;
      state.chan[c].v_goal_raw = state.chan[c].v_raw;
      state.chan[c].i_term = state.chan[c].dac;
    }

    // Check to make sure voc is sane
    UpdateVoc(c);

    // Check if we're done sweeping
    if(state.chan[c].v_raw <= (state.chan[c].vsc_raw)){
      state.chan[c].sweep_done = True;
    }else{
      state.chan[c].dac = LimitDac(state.chan[c].dac + SWEEP_DAC_STEP_SIZE);
      // Preload i term
    }
    if(state.chan[c].v_raw <= MINVOLT){
      state.chan[c].sweep_done = True;
    }
  }else{
    // hold at the MPP
    state.chan[c].v_goal_raw = state.chan[c].v_raw_sweep_mpp;
    state.chan[c].dac = state.chan[c].i_term;
  }
  ChannelOn(c);


}

/******************************************************************************/
/******************************************************************************/

#define INC_STEP_SIZE photon_tuning.inc_step

void IncrementalConductance(uint8 c){
  if(state.chan[c].v_delta == 0.0){
    if(state.chan[c].i_delta > 0.0){
      state.chan[c].v_goal_raw += INC_STEP_SIZE;
    }else if(state.chan[c].i_delta < 0.0){
      state.chan[c].v_goal_raw -= INC_STEP_SIZE;
    }
  }else if(state.chan[c].didv > -state.chan[c].iv){
    state.chan[c].v_goal_raw += INC_STEP_SIZE;
  }else if(state.chan[c].didv < -state.chan[c].iv){
    state.chan[c].v_goal_raw -= INC_STEP_SIZE;
  }
  state.chan[c].v_goal_raw = MAX(MINVOLT, MIN(MAXVOLT, state.chan[c].v_goal_raw));
}

/******************************************************************************/
/******************************************************************************/

// Power-based perturb and observe, called every 20 ms tracker tick.
// Each period: step the voltage goal, skip po_settle_ticks while the voltage
// loop settles, then average V and I (200 Hz samples) for the rest of the
// period. Reverse when the averaged power falls; grow the step while the same
// direction keeps gaining. The goal is clamped close to the measured voltage
// so that, against a source whose voltage cannot follow (e.g. a supply in
// constant voltage), the PI loop cannot wind away from the operating point.
// No Voc/Vsc window here: those come from the last sweep and go stale when the
// source changes; the sweep and periodic retrack still use them.
void PerturbObserve(uint8 c){
  int32 p, v_avg, i_avg, step, lo, hi;
  int16 tick = ++po_tick[c];

  if(tick == photon_tuning.po_settle_ticks + 1){
    po_acc_v[c] = 0;
    po_acc_i[c] = 0;
    po_acc_n[c] = 0;
    po_accumulate[c] = true;
  }
  if(tick < photon_tuning.po_period_ticks) return;

  po_accumulate[c] = false;
  po_tick[c] = 0;
  // Re-probe the learned ceiling: slowly right after a collapse, doubling
  // every 10 periods without one (so a raised source limit is followed in
  // seconds), and quickly when the loop is not pressing against it.
  if(since_collapse[c] < 1000) since_collapse[c]++;
  if(state.chan[c].i_term < soft_max[c] - 16){
    soft_max[c] += 64;
  }else{
    soft_max[c] += photon_tuning.ceiling_creep << MIN(since_collapse[c] / 10, 5);
  }
  soft_max[c] = MIN(soft_max[c], state.max_dac);
  if(po_acc_n[c] == 0) return;
  v_avg = po_acc_v[c] / po_acc_n[c];
  i_avg = po_acc_i[c] / po_acc_n[c];
  po_v_avg[c] = v_avg;
  po_i_avg[c] = i_avg;
  p = v_avg * i_avg;

  step = po_step[c];
  if(v_avg < state.chan[c].v_goal_raw - photon_tuning.po_step_min / 2){
    // The source is not reaching the goal (e.g. a supply in constant
    // voltage): power comparisons carry no information here, so always move
    // the goal below what the source actually delivers.
    state.chan[c].v_goal_raw = v_avg;
    state.chan[c].dir = TrackingDirectionEnum_DOWN;
    step = photon_tuning.po_step_min;
    po_run[c] = 0;
    po_clamp[c] = 1;
    po_near[c] = 10;
  }else{
    po_clamp[c] = 0;
    if(po_have_prev[c]){
      if(p < po_p_prev[c]){
        state.chan[c].dir = SwitchDirection(state.chan[c].dir);
        step = photon_tuning.po_step_min;
        po_run[c] = 0;
      }else if(++po_run[c] >= 2){
        step = step * 2;
      }
    }
  }
  // Large steps only while far from the operating point, and never more than
  // about 3 % of the input voltage.
  step = MIN(step, photon_tuning.po_step_max);
  step = MIN(step, MAX(photon_tuning.po_step_min, v_avg >> 5));
  // Recently found the source's limit: stay in small steps around it.
  if(po_near[c]){
    po_near[c]--;
    step = MIN(step, 2 * photon_tuning.po_step_min);
  }
  po_step[c] = step;
  po_p_prev[c] = p;
  po_have_prev[c] = 1;

  if(state.chan[c].dir == TrackingDirectionEnum_UP){
    state.chan[c].v_goal_raw += step;
  }else{
    state.chan[c].v_goal_raw -= step;
  }

  // Keep the goal near what the source actually delivers.
  hi = v_avg + photon_tuning.po_up_clamp;
  lo = v_avg - photon_tuning.po_down_clamp;
  state.chan[c].v_goal_raw = MAX(lo, MIN(hi, state.chan[c].v_goal_raw));
  state.chan[c].v_goal_raw = MAX(MINVOLT, MIN(MAXVOLT, state.chan[c].v_goal_raw));
  if(p <= ((state.chan[c].p_raw_sweep_mpp / 100) * photon_tuning.power_drop_percent)){
    MaybeInitiateRetrack();
  }
}

/******************************************************************************/
/******************************************************************************/

inline bool CheckIfVoltageInRange(int32 v_in){
  if(v_in < MINVOLT) return False;
  if(v_in > MAXVOLT) return False;  
  return True;
}

/******************************************************************************/
/******************************************************************************/

inline int32 LimitDac(int32 dac){
  if(dac > state.max_dac) return state.max_dac;
  if(dac < MINDAC) return MINDAC;
  return dac;
}
/******************************************************************************/
/******************************************************************************/

#define MAX_PID_ERROR photon_tuning.max_pid_error // ~5V 
inline int32 LimitPidError(int32 error){
  if(error > MAX_PID_ERROR) return MAX_PID_ERROR;
  if(error < -MAX_PID_ERROR) return -MAX_PID_ERROR;
  return error;
}

/******************************************************************************/
/******************************************************************************/

// Run at 200hz
void ControlVoltages(void){
  int16 c = 0;

  if(state.control == ControlEnum_TRACKING || 
      state.control == ControlEnum_TEST_CONTROL_VOLTAGE){
    DEBUG_3_TOG();
    for(c=0; c<2; c++){
      if(po_accumulate[c]){
        po_acc_v[c] += state.chan[c].v_raw;
        po_acc_i[c] += state.chan[c].i_raw;
        po_acc_n[c]++;
      }
      if(state.alg != AlgorithmEnum_SWEEP && state.alg != AlgorithmEnum_MEASURE_VOC){
        // Limit the error term to prevent integral windup and slew on the
        // controller
        state.chan[c].v_error_raw = LimitPidError(state.chan[c].v_raw -
                                                  state.chan[c].v_goal_raw);
      
        if(CheckIfVoltageInRange(state.chan[c].v_raw)){
          int32 acc;
          state.chan[c].p_term = state.chan[c].v_error_raw / photon_tuning.p_divisor;
          // Integrate in 1/256 counts so small errors still move the current.
          acc = (state.chan[c].v_error_raw * 256) / photon_tuning.i_divisor + i_frac[c];
          state.chan[c].i_term += acc >> 8;
          i_frac[c] = acc & 0xff;
          // Collapse: input below half the goal means the current command is
          // above what the source can supply (its current limit, or a sweep
          // that pushed past it). Cut 1/32 of the current every 5 ms until
          // the source recovers. The ceiling is taken once, 1.5 % below the
          // level where the collapse started, so the cuts made while the
          // source recovers do not drag it down.
          if(state.chan[c].v_raw < state.chan[c].v_goal_raw / 2){
            if(since_collapse[c] > 0){
              soft_max[c] = state.chan[c].i_term - (state.chan[c].i_term - MINDAC) / 64;
            }
            state.chan[c].i_term -= (state.chan[c].i_term - MINDAC) >> 5;
            since_collapse[c] = 0;
            i_frac[c] = 0;
          }
          if(state.chan[c].i_term > soft_max[c]){
            state.chan[c].i_term = soft_max[c];
            i_frac[c] = 0;
          }
          if(state.chan[c].i_term != LimitDac(state.chan[c].i_term)){
            state.chan[c].i_term = LimitDac(state.chan[c].i_term);
            i_frac[c] = 0;
          }
          state.chan[c].dac = LimitDac(state.chan[c].p_term +
                                     state.chan[c].i_term);
          ChannelOn(c);
        }else{
          ChannelOff(c);
          // Restart from minimum current when the input comes back, rather
          // than re-applying the current that just collapsed it.
          state.chan[c].i_term = MINDAC;
          state.chan[c].dac = MINDAC;
          PoReset(c);
          if(state.control == ControlEnum_TRACKING){
            // Trigger Retrack
            MaybeInitiateRetrack();
            
            // Jump to the measured open circuit voltage
            if(state.loops > 50){
              state.chan[c].v_goal_raw = state.chan[c].voc_raw;
            }
          }
          //state.chan[c].dac = MINDAC;
          //state.chan[c].i_term = 0;
        }
      }else{
        ChannelOn(c);
      }
    }

    SetDacValues(state.chan[0].dac, state.chan[1].dac); 
  }else if(state.control == ControlEnum_TEST_DAC_STEP ||
           state.control == ControlEnum_TEST_MODE ||
           state.control == ControlEnum_CONSTANT_VOLTAGE){
    nop();
  }else{
    for(c=0; c<2; c++){
      ChannelOff(c);
    }
  }
}

/******************************************************************************/
/******************************************************************************/

void SetDacValues(uint16 ch0, uint16 ch1){
    if(params.device.hw_rev >= PHOTON_3_2){
      nop(); 
      //SetDacVoltageMcp4728(DAC_I_REF_0, ch0);
      //SetDacVoltageMcp4728(DAC_I_REF_1, ch1);
    }else{
      CMPDAC1 = ch0 >> DAC_SHIFT;
      CMPDAC3 = ch1 >> DAC_SHIFT;
      PTCONbits.PTEN = 1;
    }
} 

