/*******************************************************************************
queue.h
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
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*******************************************************************************/

#ifndef QUEUE_H
#define QUEUE_H

#include "general.h"

/******************************************************************************/
/******************************************************************************/

#define QUEUE_MAXIMUM_LENGTH 32

/******************************************************************************/
/******************************************************************************/

struct QueueStruct
{
	size_t Buffer[QUEUE_MAXIMUM_LENGTH];
    uint16 Front;   
    uint16 Back;
    uint16 End;
    uint16 Space;
    struct tagQueueStatus {
  		unsigned BufferFull:1;
  		unsigned BufferEmpty:1;
  	} Status;
};


/******************************************************************************/
/******************************************************************************/


// Initialize the process queue.  This function must be run 
// before using the queuing system.
void QueueInitialize(void);

// Queue a process to be executed later.  Each process should be 
// represented by a function declared void and taking no 
// parameters.  The name of this function is the argument that 
// should be passed to QueuePush().  Returns 1 if the function 
// was added to the queue, 0 otherwise.
int16 QueuePush(voidfxn functionPointer);

// Clear all processes from the queue.
void QueueClear(void);

// Determine if the queue is full.  Returns 1 if the queue was 
// full, 0 otherwise.
inline int16 QueueFull(void);
inline int16 QueueEmpty(void);

// This function will check the queue to see if there are any 
// processes waiting to be run and will execute the next one in 
// line if the queue is not empty.
void QueuePump(void);

#endif
