/*******************************************************************************
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

THIS SOFTWARE IS PROVIDED BY SAMUEL LENIUS ''AS IS'' AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL SAMUEL LENIUS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*******************************************************************************/


#include "fifo.h"
#include "string.h"
#include "stdlib.h"

int pushcharfxn(fifo *f, char c);
int popcharfxn(fifo *f, char *c);
int pushstrnfxn(fifo *f, char *s, int n);
int popstrnfxn(fifo *f, char *s, int n);
int pushstrfxn(fifo *f, char *s);
int popstrfxn(fifo *f, char *s);

int pushintfxn(fifo *f, int i);
int popintfxn(fifo *f, int *i);

/******************************************************************************/
/******************************************************************************/

void initfifo(fifo *f, int size, int type){
	int i;
	f->front = 0;
	f->back = 0;
	f->flags.isEmpty = 1;
	f->flags.isFull = 0;
	f->bufsize = size;
	for(i=0; i <= f->bufsize; i++){
		f->buf[i] = 0;
	}  
	f->count = 0;
	f->pushchar = &pushcharfxn;
	f->popchar = &popcharfxn;
	f->pushstrn = &pushstrnfxn;
	f->popstrn = &popstrnfxn;
	f->pushstr = &pushstrfxn;
	f->popstr = &popstrfxn;
	f->pushint = &pushintfxn;
	f->popint = &popintfxn;
}

/******************************************************************************/
/******************************************************************************/

int pushstrfxn(fifo *f, char *s){
  int i;
  int n = strlen(s);
  for(i=0; i<n; i++){
    if(pushcharfxn(f,s[i])){
      return 1;
    }
  }
  return 0;
}

/******************************************************************************/
/******************************************************************************/

int popstrfxn(fifo *f, char *s){
  int i;
  for(i=0; i<f->bufsize; i++){
    if(f->popchar(f,&s[i])){
      return 1;
    }
    if(s[i] == '\0'){
      return 0;
    }	
  }
  return 1;
}

/******************************************************************************/
/******************************************************************************/

int pushstrnfxn(fifo *f, char *s, int n){
  int i;
  for(i=0; i<n; i++){
    if(f->pushchar(f,s[i])){
      return 1;
    }
  }
  return 0;
}

/******************************************************************************/
/******************************************************************************/

int popstrnfxn(fifo *f, char *s, int n){
  int i;
  for(i=0; i<n; i++){
    if(f->popchar(f,&s[i])){
      return 1;
    }
  }
  return 0;
}

/******************************************************************************/
/******************************************************************************/

int pushcharfxn(fifo *f, char c){
  if (f->count >= f->bufsize){
    f->flags.isFull = 1;
    return 1;
  }		
  else {
    if(++f->back >= f->bufsize) f->back = 0;
    f->buf[f->back] = c;
    f->count++;
    f->flags.isEmpty = 0;
  }
  return 0;
}

/******************************************************************************/
/******************************************************************************/

int popcharfxn(fifo *f, char *c){
  if (f->count <= 0){
    f->flags.isEmpty = 1;
    return 1;
  }
  else {
    if(++f->front >= f->bufsize) f->front = 0;
    *c = f->buf[f->front];
    f->buf[f->front] = '\0';
    f->count--;
    f->flags.isFull = 0;
  }
  if (f->count <= 0){
    f->flags.isEmpty = 1;
    return 1;
  }
  return 0;
}

/******************************************************************************/
/******************************************************************************/

int pushintfxn(fifo *f, int i){
  if (f->count >= f->bufsize){
    f->flags.isFull = 1;
    return 1;
  }else{
    if(++f->back > f->bufsize) f->back = 0;
    f->buf[f->back] = i;    
    f->count++;
    f->flags.isEmpty = 0;
  }
  return 0;	
}	

/******************************************************************************/
/******************************************************************************/

int popintfxn(fifo *f, int *i){
  if (f->count <= 0){
    f->flags.isEmpty = 1;
    return 1;
  }else{
    if(++f->front > f->bufsize) f->front = 0;
    *i = f->buf[f->front];
    f->buf[f->front] = 0;
    f->count--;
    f->flags.isFull = 0;
  }
  if (f->count <= 0){
    f->flags.isEmpty = 1;
    return 1;
  }
  return 0;
}	

/******************************************************************************/
/******************************************************************************/
