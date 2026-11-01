/*******************************************************************************
adc.c
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

#include "adc.h"
#include "general.h"
#include "i2c.h"
#include "main.h"
#include "oscillator.h"
#include "packet.h"
#include "queue.h"
#include "timer.h"
#include "tracker.h"
#include "safety.h"
#include "state.pb.h"
#include "params.pb.h"

/******************************************************************************/
/******************************************************************************/

extern volatile StateMessage state;
extern volatile ParamsMessage params;
extern volatile int therm_t[1023];
extern volatile bool run_control_voltages;
extern volatile bool run_control_voltage_fsm;
extern volatile bool send_state_message;

/******************************************************************************/
/******************************************************************************/

uint16 adc_0_arr[ADC_FILT_LEN];
uint16 adc_1_arr[ADC_FILT_LEN];
uint16 adc_2_arr[ADC_FILT_LEN];
uint16 adc_3_arr[ADC_FILT_LEN];
uint16 adc_4_arr[ADC_FILT_LEN];
uint16 adc_5_arr[ADC_FILT_LEN];
uint16 adc_6_arr[ADC_FILT_LEN];
uint16 adc_7_arr[ADC_FILT_LEN];
uint16 adc_8_arr[ADC_FILT_LEN];

/******************************************************************************/
/******************************************************************************/

uint16 buf_ptr = 0;
uint16 control_voltage_counter = 0;

/******************************************************************************/
/******************************************************************************/

void _ISR _ADCP0Interrupt (void)
{
  ADCPC0bits.SWTRG1 = 1;          // Start conversion of pair 1

  state.chan[0].v_raw += ADCBUF0;
  state.chan[0].v_raw -= adc_0_arr[buf_ptr];
  adc_0_arr[buf_ptr] = ADCBUF0;

  state.chan[0].i_raw += ADCBUF1;
  state.chan[0].i_raw -= adc_1_arr[buf_ptr];
  adc_1_arr[buf_ptr] = ADCBUF1;

  IFS6bits.ADCP0IF = 0;           // Clear ADC Pair 0 Interrupt Flag
  ADSTATbits.P0RDY = 0;           // Clear the ADSTAT bits
}

/******************************************************************************/
/******************************************************************************/

void _ISR _ADCP1Interrupt (void)
{
  ADCPC1bits.SWTRG2 = 1;        // Start conversion of pair 2
  
  state.chan[1].v_raw += ADCBUF2;
  state.chan[1].v_raw -= adc_2_arr[buf_ptr];
  adc_2_arr[buf_ptr] = ADCBUF2;

  state.chan[1].i_raw += ADCBUF3;
  state.chan[1].i_raw -= adc_3_arr[buf_ptr];
  adc_3_arr[buf_ptr] = ADCBUF3;

  IFS6bits.ADCP1IF = 0;   // Clear ADC Pair 1 Interrupt Flag
  ADSTATbits.P1RDY = 0;   // Clear the ADSTAT bits
}

/******************************************************************************/
/******************************************************************************/

void _ISR _ADCP2Interrupt (void)
{
  ADCPC1bits.SWTRG3 = 1;        // Start conversion of pair 3

  state.batt_v_raw += ADCBUF4;
  state.batt_v_raw -= adc_4_arr[buf_ptr];
  adc_4_arr[buf_ptr] = ADCBUF4;

  state.batt_i_raw += ADCBUF5;
  state.batt_i_raw -= adc_5_arr[buf_ptr];
  adc_5_arr[buf_ptr] = ADCBUF5;

  IFS7bits.ADCP2IF = 0;   // Clear ADC Pair 2 Interrupt Flag
  ADSTATbits.P2RDY = 0;   // Clear the ADSTAT bits
}

/******************************************************************************/
/******************************************************************************/

void _ISR _ADCP3Interrupt (void)
{
  ADCPC2bits.SWTRG4 = 1;        // Start conversion of pair 4
  
  state.chan[0].t_raw += ADCBUF6;
  state.chan[0].t_raw -= adc_6_arr[buf_ptr];
  adc_6_arr[buf_ptr] = ADCBUF6;

  state.chan[1].t_raw += ADCBUF7;
  state.chan[1].t_raw -= adc_7_arr[buf_ptr];
  adc_7_arr[buf_ptr] = ADCBUF7;

  IFS7bits.ADCP3IF = 0;   // Clear ADC Pair 3 Interrupt Flag
  ADSTATbits.P3RDY = 0;   // Clear the ADSTAT bits
}

/******************************************************************************/
/******************************************************************************/

void _ISR _ADCP4Interrupt (void)
{
  int16 adc = 1023 - ADCBUF8;

  state.t_raw += adc;
  state.t_raw -= adc_8_arr[buf_ptr];
  adc_8_arr[buf_ptr] = adc;

  IFS7bits.ADCP4IF = 0;   // Clear ADC Pair 4 Interrupt Flag
  ADSTATbits.P4RDY = 0;   // Clear the ADSTAT bits
  
  if(params.device.hw_rev > PHOTON_3_0){
    // These revs use thermistors - way better then the i2c sensor.
    state.temp_ambient = therm_t[state.t_raw >> ADC_FILT_SHIFT];
    state.chan[0].temp = therm_t[state.chan[0].t_raw >> ADC_FILT_SHIFT];
    state.chan[1].temp = therm_t[state.chan[1].t_raw >> ADC_FILT_SHIFT];
  }

  buf_ptr++;
  buf_ptr &= ADC_FILT_MASK;
  
  // This fires at 10kHz
  if(params.device.hw_rev >= PHOTON_3_2){ 
    Mcp4728ControlVoltageFSM();
  }

}


/******************************************************************************/
/******************************************************************************/

inline uint16 CalculateComparatorThreshold(uint16 out, uint16 in){
  if(in > 150){
    return (out - ((out - in) / 2));
  }else{
    return 150;
  }
} 

/******************************************************************************/
/******************************************************************************/

void InitAdc(void){
  ADCONbits.FORM = 0;           // Output in Integer Format
  ADCONbits.EIE = 0;            // Enable Early Interrupt
  ADCONbits.ORDER = 0;          // Normal Order of conversion
  ADCONbits.SEQSAMP = 0;        // Simultaneous sampling
  ADCONbits.ASYNCSAMP = 0;      // Synchronous sampling
  ADCONbits.SLOWCLK = 0;        // High Frequency Clock input
  ADCONbits.ADCS = 5;           // Clock divider selection
  ADCPC0bits.IRQEN0 = 1;        // Enable common ADC Interrupt for Pair 0
  
  ADPCFGbits.PCFG0 = 0;         // AN0 is configured as analog input
  ADPCFGbits.PCFG1 = 0;         // AN1 is configured as analog input
  ADPCFGbits.PCFG2 = 0;         // AN2 is configured as analog input
  ADPCFGbits.PCFG3 = 0;         // AN3 is configured as analog input
  ADPCFGbits.PCFG4 = 0;         // AN4 is configured as analog input
  ADPCFGbits.PCFG5 = 0;         // AN5 is configured as analog input
  ADPCFGbits.PCFG6 = 0;         // AN6 is configured as analog input
  ADPCFGbits.PCFG7 = 0;         // AN7 is configured as analog input
  ADPCFGbits.PCFG8 = 0;         // AN8 is configured as analog input

  IPC27bits.ADCP0IP = 0x03;     // Set ADC Pair 0 Interrupt Priority (Level 3)
  IPC27bits.ADCP1IP = 0x03;     // Set ADC Pair 1 Interrupt Priority (Level 3)
  IPC28bits.ADCP2IP = 0x03;     // Set ADC Pair 2 Interrupt Priority (Level 3)
  IPC28bits.ADCP3IP = 0x03;     // Set ADC Pair 3 Interrupt Priority (Level 3)
  IPC28bits.ADCP4IP = 0x03;     // Set ADC Pair 4 Interrupt Priority (Level 3)
  
  IFS6bits.ADCP0IF = 0;         // Clear ADC Pair 0 Interrupt Flag
  IFS6bits.ADCP1IF = 0;         // Clear ADC Pair 1 Interrupt Flag
  IFS7bits.ADCP2IF = 0;         // Clear ADC Pair 2 Interrupt Flag
  IFS7bits.ADCP3IF = 0;         // Clear ADC Pair 3 Interrupt Flag
  IFS7bits.ADCP4IF = 0;         // Clear ADC Pair 4 Interrupt Flag
  
  IEC6bits.ADCP0IE = 1;         // Enable ADC Pair 0 Interrupt  
  IEC6bits.ADCP1IE = 1;         // Enable ADC Pair 1 Interrupt
  IEC7bits.ADCP2IE = 1;         // Enable ADC Pair 2 Interrupt
  IEC7bits.ADCP3IE = 1;         // Enable ADC Pair 3 Interrupt
  IEC7bits.ADCP4IE = 1;         // Enable ADC Pair 4 Interrupt
  
  ADSTATbits.P0RDY = 0;         //Clear Pair 0 data ready bit
  ADSTATbits.P1RDY = 0;         //Clear Pair 1 data ready bit
  ADSTATbits.P2RDY = 0;         //Clear Pair 2 data ready bit
  ADSTATbits.P3RDY = 0;         //Clear Pair 3 data ready bit
  ADSTATbits.P4RDY = 0;         //Clear Pair 4 data ready bit
  
  ADCPC0bits.IRQEN0 = 1;        //Enable ADC Interrupt pair 0 
  ADCPC0bits.IRQEN1 = 1;        //Enable ADC Interrupt pair 1
  ADCPC1bits.IRQEN2 = 1;        //Enable ADC Interrupt pair 2 
  ADCPC1bits.IRQEN3 = 1;        //Enable ADC Interrupt pair 3 
  ADCPC2bits.IRQEN4 = 1;        //Enable ADC Interrupt pair 4 
  
  ADCPC0bits.TRGSRC0 = 0b01100; //ADC Pair 0 triggered by PWM1 Trigger
  ADCPC0bits.TRGSRC1 = 0b00001; //ADC Pair 1 triggered by Software Trigger
  ADCPC1bits.TRGSRC2 = 0b00001; //ADC Pair 2 triggered by Software Trigger
  ADCPC1bits.TRGSRC3 = 0b00001; //ADC Pair 3 triggered by Software Trigger
  ADCPC2bits.TRGSRC4 = 0b00001; //ADC Pair 4 triggered by Software Trigger

  // Run ADC interrupt at 10khz
  InitTimer(1, 4000, TIMER_1to1_PRESCALE | TIMER_ON, IPL_3 | INTERRUPT_ENABLED);
  
  ADCONbits.ADON = 1;
}

/******************************************************************************/
/******************************************************************************/

