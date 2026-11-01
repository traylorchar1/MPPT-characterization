/*******************************************************************************
   cobs.c
   Copyright (c) 2015, Samuel Lenius
   All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:
* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.
* Neither the name of DILITHIUM POWER SYSTEMS LLC nor the names of its
  contributors may be used to endorse or promote products derived from this
  software without specific prior written permission.

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

#include "general.h"

// Code adapted from:
// http://conferences.sigcomm.org/sigcomm/1997/papers/p062.pdf

/******************************************************************************/
/******************************************************************************/

#define FinishBlock(X) (*code_ptr = (X), code_ptr = dst++, code = 0x01)

void CobsEncode(const uint8 *ptr, uint16 length, uint8 *dst){
  const uint8 *end = ptr + length;
  uint8 *code_ptr = dst++;
  uint8 code = 0x01;

  while (ptr < end){
    if (*ptr == 0) FinishBlock(code);
    else{
      *dst++ = *ptr;
      code++;
      if (code == 0xFF) FinishBlock(code);
    }
    ptr++;
  }
  FinishBlock(code);
}

/******************************************************************************/
/******************************************************************************/

void CobsDecode(const uint8 *ptr, uint16 length, uint8 *dst){
  const uint8 *end = ptr + length;
  while (ptr < end){
    int i, code = *ptr++;
    for (i=1; ptr<end && i<code; i++){
      *dst++ = *ptr++;
    }
    if (code < 0xFF){
      *dst++ = 0;
    }
  }
}

/******************************************************************************/
/******************************************************************************/

