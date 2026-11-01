/*******************************************************************************
periph_compat.c
Replacements for the legacy Microchip dsPIC peripheral library calls used by
this firmware. The bundled libp33FJ64GS606-coff.a is COFF and XC16 2.x links
ELF only. Each function mirrors the documented behaviour of the original
library routine.
*******************************************************************************/

#include "general.h"

/******************************************************************************/
/* SPI */

void OpenSPI1(unsigned int config1, unsigned int config2, unsigned int config3){
  SPI1CON1 = config1;
  SPI1CON2 = config2;
  SPI1STAT = config3;
}

void OpenSPI2(unsigned int config1, unsigned int config2, unsigned int config3){
  SPI2CON1 = config1;
  SPI2CON2 = config2;
  SPI2STAT = config3;
}

// config bit 3 = interrupt enable, bits 0-2 = priority
void ConfigIntSPI1(unsigned int config){
  IFS0bits.SPI1IF = 0;
  IPC2bits.SPI1IP = config & 0x0007;
  IEC0bits.SPI1IE = (config & 0x0008) >> 3;
}

void ConfigIntSPI2(unsigned int config){
  IFS2bits.SPI2IF = 0;
  IPC8bits.SPI2IP = config & 0x0007;
  IEC2bits.SPI2IE = (config & 0x0008) >> 3;
}

char DataRdySPI1(void){ return SPI1STATbits.SPIRBF; }
char DataRdySPI2(void){ return SPI2STATbits.SPIRBF; }

unsigned int ReadSPI1(void){
  if(SPI1CON1bits.MODE16) return SPI1BUF;
  return SPI1BUF & 0xff;
}

unsigned int ReadSPI2(void){
  if(SPI2CON1bits.MODE16) return SPI2BUF;
  return SPI2BUF & 0xff;
}

void WriteSPI1(unsigned int data_out){
  if(SPI1CON1bits.MODE16) SPI1BUF = data_out;
  else SPI1BUF = data_out & 0xff;
}

void WriteSPI2(unsigned int data_out){
  if(SPI2CON1bits.MODE16) SPI2BUF = data_out;
  else SPI2BUF = data_out & 0xff;
}

/******************************************************************************/
/* I2C1 */

void IdleI2C1(void){
  while(I2C1CONbits.SEN || I2C1CONbits.PEN || I2C1CONbits.RCEN ||
        I2C1CONbits.RSEN || I2C1CONbits.ACKEN || I2C1STATbits.TRSTAT);
}

void StartI2C1(void){ I2C1CONbits.SEN = 1; }
void StopI2C1(void){ I2C1CONbits.PEN = 1; }

char MasterWriteI2C1(unsigned char data_out){
  I2C1TRN = data_out;
  if(I2C1STATbits.IWCOL) return -1;
  return 0;
}

unsigned char MasterReadI2C1(void){
  I2C1CONbits.RCEN = 1;
  while(I2C1CONbits.RCEN);
  I2C1STATbits.I2COV = 0;
  return I2C1RCV;
}

/******************************************************************************/
/* SMPS PWM and analog comparators (used only on hardware before rev 3.2) */

void CloseSmpsPWM(void){
  PTCONbits.PTEN = 0;
}

void ConfigIntSmpsCMP1(unsigned int cmp_intp, unsigned int pri){
  IFS1bits.AC1IF = 0; IPC4bits.AC1IP = pri & 7; IEC1bits.AC1IE = cmp_intp & 1;
}

void ConfigIntSmpsCMP2(unsigned int cmp_intp, unsigned int pri){
  IFS6bits.AC2IF = 0; IPC25bits.AC2IP = pri & 7; IEC6bits.AC2IE = cmp_intp & 1;
}

void ConfigIntSmpsCMP3(unsigned int cmp_intp, unsigned int pri){
  IFS6bits.AC3IF = 0; IPC26bits.AC3IP = pri & 7; IEC6bits.AC3IE = cmp_intp & 1;
}

void ConfigIntSmpsCMP4(unsigned int cmp_intp, unsigned int pri){
  IFS6bits.AC4IF = 0; IPC26bits.AC4IP = pri & 7; IEC6bits.AC4IE = cmp_intp & 1;
}
