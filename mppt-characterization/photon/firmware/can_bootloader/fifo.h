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

#ifndef FIFO_H
#define FIFO_H

//typedef struct fifotag fifo;

typedef struct fifotag fifo;

#define FIFO_LENGTH	255

struct fifotag{
		//int * buf;	//buffer pointer
		char buf[FIFO_LENGTH];	//buffer array
		int bufsize;
        int front;		//pointer to start of buffer
        int back;		//pointer to back of buffer
        int count;		//number of elements in buffer
		struct{
			unsigned isEmpty :1;
			unsigned isFull :1;
		}flags;
		int (*pushchar)(fifo *f, char c);
		int (*popchar)(fifo *f, char *c);

		int (*pushstrn)(fifo *f, char *s, int n);
		int (*popstrn)(fifo *f, char *s, int n);

		int (*pushstr)(fifo *f, char *s);
		int (*popstr)(fifo *f, char *s);

		int (*pushint)(fifo *f, int i);
		int (*popint)(fifo *f, int *i);

};

void initfifo(fifo *f, int size, int type);

#endif
