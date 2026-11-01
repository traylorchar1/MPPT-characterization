#include "Generic.h"
#include "peripheralversion.h"


#if defined(__dsPIC33F__)
#include <p33Fxxxx.h>
#else
#error "Does not build on this target"
#endif

#ifndef __CTMU_H
#define __CTMU_H

#ifdef _CTMUIF
	
#define CTMUCON1_VALUE					0x0000
#define CTMUCON2_VALUE					0x0000
#define CTMUICON_VALUE					0x0000

/*----------------------------------------------------------------------------------------------------*/
/* CTMUCON1 Configuration Bit Definitions : */
/*----------------------------------------------------------------------------------------------------*/

#define CTMU_ENABLE					0xFFFF
#define CTMU_DISABLE					0x7FFF

#define CTMU_STOP_IDLE					0xFFFF
#define CTMU_CONTINUE_IDLE				0xDFFF

#define CTMU_DELAY_ENABLE				0xFFFF
#define CTMU_DELAY_DISABLE				0xEFFF

#define CTMU_EDGE_NOT_BLOCKED				0xFFFF
#define CTMU_EDGE_BLOCKED				0xF7FF

#define CTMU_EDGE_EVENT					0xFFFF
#define CTMU_NO_EDGE_EVENT				0xFBFF

#define CTMU_ANALOG_GROUND				0xFFFF
#define CTMU_ANALOG_NO_GROUND				0xFDFF

#define CTMU_TRIG_ENABLE				0xFFFF
#define CTMU_TRIG_DISABLE				0xFEFF

/*----------------------------------------------------------------------------------------------------*/
/* CTMUCON2 Configuration Bit Definitions : */
/*----------------------------------------------------------------------------------------------------*/

#define CTMU_EDGE1_EDGE					0xFFFF
#define CTMU_EDGE1_LEVEL				0x7FFF

#define CTMU_EDGE1_POSITIVE				0xFFFF
#define CTMU_EDGE1_NEGATIVE				0xFBFF

#define CTMU_EDGE1_SOURCE_TIMER1			0xFCFF
#define CTMU_EDGE1_SOURCE_OC1				0xFDFF
#define CTMU_EDGE1_SOURCE_CTED2				0xFEFF
#define CTMU_EDGE1_SOURCE_CTED1				0xFFFF

#define CTMU_EDGE2_OCCUR				0xFFFF
#define CTMU_EDGE2_NOT_OCCUR				0xFDFF

#define CTMU_EDGE1_OCCUR				0xFFFF
#define CTMU_EDGE1_NOT_OCCUR				0xFEFF

#define CTMU_EDGE2_EDGE					0xFFFF
#define CTMU_EDGE2_LEVEL				0xFF7F

#define CTMU_EDGE2_POSITIVE				0xFFFF
#define CTMU_EDGE2_NEGATIVE				0xFFBF

#define CTMU_EDGE2_SOURCE_IC1				0xFFCF
#define CTMU_EDGE2_SOURCE_CMP2				0xFFDF
#define CTMU_EDGE2_SOURCE_CTED1				0xFFEF
#define CTMU_EDGE2_SOURCE_CTED2				0xFFFF		


/*----------------------------------------------------------------------------------------------------*/
/* CTMUICON Configuration Bit Definitions : */
/*----------------------------------------------------------------------------------------------------*/

#define CTMU_CURR_RANGE_100_BASE_CURR 			0xFFFF /*Current source Range is 100*Base current (55uA) */
#define CTMU_CURR_RANGE_10_BASE_CURR  			0xFEFF /*Current source Range is 10*Base current (5.5uA)*/
#define CTMU_CURR_RANGE_BASE_CURR     			0xFDFF /*Current source Range is Base current (0.55uA)*/
#define CTMU_CURR_SOURCE_DISABLE     			0xFCFF /*Current source disabled*/

#define CTMU_CURR_TRIM_MAX_POS				0x7FFF /* Maximum positive change from nominal current */
#define CTMU_CURR_TRIM_MIN_POS				0x04FF /* Minimum positive change from nominal current */
#define CTMU_CURR_TRIM_NOMINAL				0x03FF /* Nominal current output specified by IRNG<1:0> */
#define CTMU_CURR_TRIM_MIN_NEG				0xFFFF /* Minimum negative change from nominal current */
#define CTMU_CURR_TRIM_MAX_NEG				0x84FF /* Maximum negative change from nominal current */

/************************************************************************
Macro       : Enbl_CTMUTGEN
Overview    : By setting TGEN bit, edge delay generation is enabled  
Parameters  : None 
Remarks     : None.
**************************************************************************/ 
#define Enbl_CTMUTGEN  				(CTMUCON1bits.TGEN = 1)

/************************************************************************
Macro       : Enbl_CTMUTRIG
Overview    : By setting CTTRIG bit, Trigger output is enabled 
Parameters  : None 
Remarks     : None.
**************************************************************************/ 
#define Enbl_CTMUTRIG 				(CTMUCON1bits.CTTRIG = 1)

/************************************************************************
Macro       : Enbl_EDGE
Overview    : By setting EDGEN bit, edges are not blocked  
Parameters  : None 
Remarks     : None.
**************************************************************************/ 
#define Enbl_EDGE  				(CTMUCON1bits.EDGEN = 1)

/************************************************************************
Macro       : Disbl_EDGE
Overview    : By clearing EDGEN bit, edges are blocked 
Parameters  : None 
Remarks     : None.
**************************************************************************/ 
#define Enbl_CTMUTRIG 				(CTMUCON1bits.CTTRIG = 1)

/************************************************************************
Macro        :Disbl_CTMUTGEN
Overview    : By clearing TGEN bit, disable the edge delay generation
Parameters  : None 
Remarks     : None.
**************************************************************************/ 
#define Disbl_CTMUTGEN 				(CTMUCON1bits.TGEN = 0)

/************************************************************************
Macro       :Disbl_CTMUTRIG
Overview    : By clearing CTTRIG bit disable the Trigger output 
Parameters  : None 
Remarks     : None.
**************************************************************************/ 
#define Disbl_CTMUTRIG				(CTMUCON1bits.CTTRIG = 0)

/************************************************************************
Macro       : CTMUEdge1_Status
Overview    : Returns the status of CTMU edge1  
Parameters  : None 
Remarks     : None.
**************************************************************************/ 
#define CTMUEdge1_Status      			(CTMUCON2bits.EDGE1)

/************************************************************************
Macro       : CTMUEdge2_Status
Overview    : Returns the status of CTMU edge2 
Parameters  : None 
Remarks     : None.
**************************************************************************/ 
#define CTMUEdge2_Status  	 		(CTMUCON2bits.EDGE2)

/***********************************************************************
Macro      : EnableIntCTMU 
Overview   : This macro enables the CTMU interrupt. 
Parameters : None 
Remarks    : This macro sets CTMU Interrupt Enable bit of Interrupt 
                  Enable Control Register.
************************************************************************/ 
#define EnableIntCTMU                		(IEC4bits.CTMUIE = 1)

/***********************************************************************
Macro      : DisableIntCTMU 
Overview   : This macro disables the CTMU interrupt.
Parameters : None
Remarks    : This macro clears CTMU Interrupt Enable bit of Interrupt 
             Enable Control register.
************************************************************************/ 
#define DisableIntCTMU               		(IEC4bits.CTMUIE = 0)

/***********************************************************************
Macro      : SetPriorityIntCTMU(priority) 
Overview   : This macro sets priority for CTMU interrupt. 
Parameters : priority - This input parameter is the level of interrupt priority 
Remarks    : This macro sets CTMU Interrupt Priority bits of Interrupt 
             Priority Control register.
************************************************************************/  
#define SetPriorityIntCTMU(priority)    	(IPC19bits.CTMUIP = priority)

/*******************************************************************
Macro       : CTMU_Clear_Intr_Status_Bit 
Overview    : Macro to Clear CTMU Interrupt Status bit 
Parameters  : None 
Remarks     : None 
*******************************************************************/

#define CTMU_Clear_Intr_Status_Bit    		(IFS4bits.CTMUIF = 0) 
  
void __attribute__ ((section (".libperi")))ConfigIntCTMU(unsigned int config);

void __attribute__ ((section (".libperi")))OpenCTMU(unsigned int config1, unsigned int config2, unsigned int config3);

void __attribute__ ((section (".libperi")))CurrentControlCTMU(unsigned int config);

void __attribute__ ((section (".libperi")))CloseCTMU(void);

#endif 
 
#endif 

