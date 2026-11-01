/*******************************************************************************
memory.c
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

#include "prog_memory.h"
#include "general.h"

#define COMMAND_NACK     0x00
#define COMMAND_ACK      0x01
#define COMMAND_READ_PM  0x02
#define COMMAND_WRITE_PM 0x03
#define COMMAND_WRITE_CM 0x07
#define COMMAND_RESET    0x08
#define COMMAND_READ_ID  0x09

#define PM_ROW_SIZE 64 * 8
#define CM_ROW_SIZE 8
#define CONFIG_WORD_SIZE 1

#define PM_ROW_ERASE     0x4042
#define PM_ROW_WRITE     0x4001
#define CONFIG_WORD_WRITE  0X4000

#define FCY   40000000
#define BAUDRATE        115200                   
#define BRGVAL          ((FCY/BAUDRATE)/16)-1 

/******************************************************************************/
/******************************************************************************/
// Interface with memory.s
extern void Erase(uint16, uint16, uint16);
extern uint32 ReadLatch(uint16, uint16);
extern void WriteLatch(uint16, uint16, uint16, uint16);
extern void WriteMem(uint16);

/******************************************************************************/
/******************************************************************************/

void PutChar(char);
void GetChar(char *);
void WriteBuffer(char *, int);

char Buffer[128];

int main_blop(void)

{

    union u_t SourceAddr;
  union u_t Delay;
  
  SourceAddr.u32 = 0xc00;

  Delay.u32 = ReadLatch(SourceAddr.su.hu, SourceAddr.su.lu);

  if(Delay.cu[0] == 0)
  {
  }

  T2CONbits.T32 = 1; /* to increment every instruction cycle */
  IFS0bits.T3IF = 0; /* Clear the Timer3 Interrupt Flag */
  IEC0bits.T3IE = 0; /* Disable Timer3 Interrup Service Routine */

  if((Delay.u32 & 0x000000FF) != 0xFF)
  {
    /* Convert seconds into timer count value */
    Delay.u32 = ((uint32)(FCY)) * ((uint32)(Delay.cu[0]));

    PR3 = Delay.su.hu;
    PR2 = Delay.su.lu;

    /* Enable Timer */
    T2CONbits.TON=1;
  }


  U2BRG = BRGVAL;      /*  BAUD Rate Setting of Uart2  (0XEF for 9600)*/


  U2MODE = 0x8000; /* Reset UART to 8-n-1, alt pins, and enable */
  U2STA  = 0x0400; /* Reset status register and enable TX */
   


  while(1)
  {
    char Command;

    GetChar(&Command);

    switch(Command)
    {
      case COMMAND_READ_PM:        /*tested*/
      {
        union u_t SourceAddr;

        GetChar(&(SourceAddr.cs[0]));
        GetChar(&(SourceAddr.cs[1]));
        GetChar(&(SourceAddr.cs[2]));
        SourceAddr.cu[3]=0;

        //ReadPmRow(Buffer, SourceAddr);

        WriteBuffer(Buffer, PM_ROW_SIZE*3);

        break;
      }

      case COMMAND_WRITE_PM:        /* tested */
      {
          union u_t SourceAddr;
        int    Size;

        GetChar(&(SourceAddr.cs[0]));
        GetChar(&(SourceAddr.cs[1]));
        GetChar(&(SourceAddr.cs[2]));
        SourceAddr.cu[3]=0;
        
        for(Size = 0; Size < PM_ROW_SIZE*3; Size++)
        {
        GetChar(&(Buffer[Size]));
        }

        Erase(SourceAddr.su.hu,SourceAddr.su.lu,PM_ROW_ERASE);
    
        //WritePmRow(Buffer, SourceAddr);    /*program page */

        PutChar(COMMAND_ACK);        /*Send Acknowledgement */

         break;
      }

      case COMMAND_READ_ID:
      {
        union u_t SourceAddr;
        union u_t Temp;

        SourceAddr.u32 = 0xFF0000;

        Temp.u32 = ReadLatch(SourceAddr.su.hu, SourceAddr.su.lu);

        WriteBuffer(&(Temp.cs[0]), 4);

        SourceAddr.u32 = 0xFF0002;

        Temp.u32 = ReadLatch(SourceAddr.su.hu, SourceAddr.su.lu);

        WriteBuffer(&(Temp.cs[0]), 4);

        break;
      }
      
      case COMMAND_WRITE_CM:  
      {
        int    Size;

        for(Size = 0; Size < CM_ROW_SIZE*3;)
        {
          GetChar(&(Buffer[Size++]));
          GetChar(&(Buffer[Size++]));
          GetChar(&(Buffer[Size++]));
        
          PutChar(COMMAND_ACK);        /*Send Acknowledgement */
        }

        
        break;
      }
        case COMMAND_RESET:
      {
        union u_t SourceAddr;
        int    Size;
        union u_t Temp;
        

        for(Size = 0, SourceAddr.u32 = 0xF80000; Size < CM_ROW_SIZE*3; 
                                Size +=3, SourceAddr.u32 += 2)
        {
          if(Buffer[Size] == 0)
          {
            Temp.cu[0]=Buffer[Size+1];
            Temp.cu[1]=Buffer[Size+2];

            WriteLatch( SourceAddr.su.hu,
                    SourceAddr.su.lu,
                    Temp.su.hu,
                    Temp.su.lu);

            WriteMem(CONFIG_WORD_WRITE);
          }
        }
        


        break;
      }

      case COMMAND_NACK:
      {

        break;
      }

      
      default:
        PutChar(COMMAND_NACK);
        break;
    }

  }

}

/******************************************************************************/
void GetChar(char * ptrChar)
{
  while(1)
  {  
    /* if timer expired, signal to application to jump to user code */
    if(IFS0bits.T3IF == 1)
    {
      * ptrChar = COMMAND_NACK;
      break;
    }
    /* check for receive errors */
    if(U2STAbits.FERR == 1)
    {
      continue;
    }
      
    /* must clear the overrun error to keep uart receiving */
    if(U2STAbits.OERR == 1)
    {
      U2STAbits.OERR = 0;
      continue;
    }

    /* get the data */
    if(U2STAbits.URXDA == 1)
    {
      T2CONbits.TON=0; /* Disable timer countdown */
      * ptrChar = U2RXREG;
      break;
    }
  }
}


/******************************************************************************/
union u_t ReadPmLatch(union u_t addr)
{
  union u_t read_value;
  
  read_value.u32 = ReadLatch(addr.su.hu, addr.su.lu);

  return read_value;
}
/******************************************************************************/

void WriteBuffer(char * ptrData, int Size)
{
  int DataCount;
  
  for(DataCount = 0; DataCount < Size; DataCount++)
  {
    PutChar(ptrData[DataCount]);
  }
}
/******************************************************************************/
void PutChar(char Char)
{
}
/******************************************************************************/

void WritePmLatch(union u_t addr, union u_t data)
{
  WriteLatch(addr.su.hu, addr.su.lu, data.su.hu, data.su.lu);
} 

/******************************************************************************/

void WritePmBlock(void){
  WriteMem(PM_ROW_WRITE); 
}

/******************************************************************************/

void ErasePmBlock(union u_t addr){
  Erase(addr.su.hu, addr.su.lu, PM_ROW_ERASE);
}

/******************************************************************************/

