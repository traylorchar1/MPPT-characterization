/*******************************************************************************
queue.c
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

#include "queue.h"

static volatile struct QueueStruct Queue;

/******************************************************************************/
/******************************************************************************/

inline int16 QueueEmpty()
{
	
    return (Queue.Front == Queue.Back);

}

/******************************************************************************/
/******************************************************************************/

void QueueInitialize(void)
{
	unsigned short int i;
	
	Queue.End = QUEUE_MAXIMUM_LENGTH;
	
	// Set the queue to empty
	for(i = 0; i < Queue.End; i++) Queue.Buffer[i] = 0;
	Queue.Front = 0;
	Queue.Back  = 0;

}

/******************************************************************************/
/******************************************************************************/

void QueueClear(void)
{
	Queue.Front = 0;
	Queue.Back = 0;

}

/******************************************************************************/
/******************************************************************************/

inline int16 QueueFull(void)
{
    // If queueBack is one less than queueFront, queue is full
    if (Queue.Front == Queue.Back + 1) 						return TRUE;
	if (Queue.Front == 0 && Queue.Back >= Queue.End - 1) 	return TRUE;
    														return FALSE;
}

/******************************************************************************/
/******************************************************************************/

inline void QueueCalculateBufferSpace(void){
	
	
	if( Queue.Front <= Queue.Back)	Queue.Space = Queue.End + Queue.Front - Queue.Back - 1;
	else							Queue.Space = Queue.Front - Queue.Back - 1;		
	
	if(Queue.Space == 0) 			{Queue.Status.BufferFull = 1; return;}
	else 							Queue.Status.BufferFull = 0;
		
	if(Queue.Space == Queue.End -1)	{Queue.Status.BufferEmpty = 1; return;}
	else							Queue.Status.BufferEmpty = 0;
	
}	

/******************************************************************************/
/******************************************************************************/

int16 QueuePush(voidfxn functionPointer)
{
	size_t functionAddress = 0;
	
	disableInterrupts();

	QueueCalculateBufferSpace();

	// Return if the queue is full
	//if (QueueFull()) return FALSE;
	if( Queue.Status.BufferFull ) {enableInterrupts(); return False;}	
	
	// Convert from function type to 32-bit integer
	functionAddress = (size_t)functionPointer;
	
	// Save functionAddress to queue
	Queue.Buffer[Queue.Back++] = functionAddress;
	
	//disableInterrupts;
	
	// Increment back pointer to reflect new entry in queue
    if (Queue.Back >= Queue.End) Queue.Back = 0;
	
	// Make sure to reenable the interrupts
	
	restoreInterrupts();
	
	return TRUE;
}

/******************************************************************************/
/******************************************************************************/

void QueuePump(void)
{
	size_t functionAddress;
	
	QueueCalculateBufferSpace();
	
	// Return if the queue is empty
	//if (QueueEmpty()) return;
	if( Queue.Status.BufferEmpty ) return;
	
    // Retrieve the address of the function from the queue
	functionAddress = Queue.Buffer[Queue.Front++];
		
	// Increment front pointer to reflect entry pulled out of queue
    if (Queue.Front >= Queue.End) Queue.Front = 0;
	
	// Execute queued function
	((void (*)(void))functionAddress)();
}

/******************************************************************************/
/******************************************************************************/
