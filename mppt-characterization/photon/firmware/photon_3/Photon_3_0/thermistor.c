/*******************************************************************************
thermistor.c
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

#include <math.h>

#include "general.h"
#include "main.h"
#include "params.pb.h"

extern volatile ParamsMessage params;
extern volatile int therm_t[1023];

/******************************************************************************/
/******************************************************************************/

#define KELVIN_TO_CELCIUS       (-273.15)
#define CELCIUS_TO_KELVIN       (273.15)
#define ADC_BIAS_V              (3.30)

/******************************************************************************/
/******************************************************************************/

void InitThermistor(void){
  // InitThermsitor takes no arguements and initializes the therm_t table.
  // This function uses the parameters of the params struct as the basis for
  // the thermsitor parameters. See:
  // https://en.wikipedia.org/wiki/Thermistor#B_or_.CE.B2_parameter_equation
  
  float r_inf;
  float thermsitor_resistance;
  float thermsitor_voltage;
  int adc_codes = 1024;
  int adc_code;
  float kelvin;
  float celcius;  
        
  r_inf = params.thermistor.r_o * exp(-params.thermistor.beta /
                  (params.thermistor.t_o + CELCIUS_TO_KELVIN)); 
        
  // For each ADC code, compute thermistor voltage and adc voltage, then
  // compute the resulting temperature. Store those themperatures in
  //  centidegrees limited to (-27315 - INT16MAX)
  for(adc_code = 0; adc_code < adc_codes; adc_code++){
    thermsitor_voltage = (float)adc_code/((float)adc_codes-1)*ADC_BIAS_V;
    thermsitor_resistance = ADC_BIAS_V *
      params.thermistor.r_bias / 
      thermsitor_voltage - params.thermistor.r_bias;
    kelvin = params.thermistor.beta / (log(thermsitor_resistance / r_inf));
    celcius = kelvin + KELVIN_TO_CELCIUS;
    if(celcius >= INT16MAX) celcius = INT16MAX;
    if(celcius <= -273.15) celcius = -273.15;
    therm_t[adc_code] = celcius * 100;
  }       
}       

/******************************************************************************/
/******************************************************************************/

