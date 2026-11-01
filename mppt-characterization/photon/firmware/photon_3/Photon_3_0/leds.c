/*******************************************************************************
leds.c
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

#include "general.h"
#include "leds.h"
#include "params.pb.h"

/******************************************************************************/
/******************************************************************************/

extern volatile ParamsMessage params;
static bool new_mode = False;

/******************************************************************************/
/******************************************************************************/

void InitLEDs(void){
  //Turn all the LEDs off
  if(params.device.hw_rev > PHOTON_3_0){
    new_mode = True;
  }else{
    new_mode = False;
  }
  LED_G_OFF();
  LED_R_OFF();
  enableLEDs(ON);
}

/******************************************************************************/
/******************************************************************************/

inline void LED_G_ON(void){
  if(new_mode){ LED_G_1 = ON;  }
  else{         LED_G_0 = ON;  }
}

/******************************************************************************/
/******************************************************************************/

inline void LED_R_ON(void){
  if(new_mode){ LED_R_1 = ON;  }
  else{         LED_R_0 = ON;  }
}

/******************************************************************************/
/******************************************************************************/

inline void LED_G_OFF(void){
  if(new_mode){ LED_G_1 = OFF;  }
  else{         LED_G_0 = OFF;  }
}

/******************************************************************************/
/******************************************************************************/

inline void LED_R_OFF(void){
  if(new_mode){ LED_R_1 = OFF;  }
  else{         LED_R_0 = OFF;  }
}

/******************************************************************************/
/******************************************************************************/

inline void LED_G_TOG(void){
  if(new_mode){ LED_G_REG_1 ^= LED_G_MASK_1;  }
  else{         LED_G_REG_0 ^= LED_G_MASK_0;  }
}

/******************************************************************************/
/******************************************************************************/

inline void LED_R_TOG(void){
  if(new_mode){ LED_R_REG_1 ^= LED_R_MASK_1;  }
  else{         LED_R_REG_0 ^= LED_R_MASK_0;  }
}


/******************************************************************************/
/******************************************************************************/


void enableLEDs(int state){
  if(state == ON){
    //Set port bits to outputs
    if(new_mode){
      LED_G_TRIS_1 = 0;
      LED_R_TRIS_1 = 0;
    }else{
      LED_G_TRIS_0 = 0;
      LED_R_TRIS_0 = 0;
    }
  }else{
    //Set port bits to inputs
    if(new_mode){
      LED_G_TRIS_1 = 1;
      LED_R_TRIS_1 = 1;
    }else{
      LED_G_TRIS_0 = 1;
      LED_R_TRIS_0 = 1;
    }
  }
}

