/*******************************************************************************
tracker.h
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

#ifndef TRACKER_H
#define TRACKER_H

#include "general.h"

/******************************************************************************/
/******************************************************************************/
/*Channel ID*/
//#define TRACKER_CHANNEL	4

/*Mode Switches*/

//#define CAL_MODE

/***Tracking Parameters***/
//number of seconds to run tracking algorithm before curvetracing
//#define TRACK_SECONDS	300
//#define TRACK_SECONDS	45
//#define TRACK_LOOPS		(TRACK_SECONDS * STATE_FREQ)


/***Tracker Modes***/
//LEVEL_0_TRACK_MODE	//hold mpp - works
//LEVEL_1_TRACK_MODE	//simple derivative tracker	- works
//LEVEL_2_TRACK_MODE	//derivative tracker with hysteresis - works
//lEVEL_3_TRACK_MODE	//voltage tracker - doesnt work

/******************************************************************************/
/******************************************************************************/
/*Macros*/
#define TRACK_UP	1
#define TRACK_DOWN	0

#define TRACKER_INIT				0
#define TRACKER_CAP_CHARGE			1
#define TRACKER_CURVETRACE			2
#define TRACKER_TRACK				3
#define TRACKER_OVERVOLT_ERROR		4
#define TRACKER_UNDERVOLT_ERROR		5
#define TRACKER_CURR_ERROR			6
#define TRACKER_TEMP_ERROR			7
#define TRACKER_BOOST_ERROR			8
#define TRACKER_POWER_ERROR			9
#define TRACKER_WAIT_RESTART		10
#define TRACKER_BATT_CHARGE_ERROR 	11
#define TRACKER_DI_OFF				12
#define TRACKER_TEST_COMMAND		13

#define TRACKER_KILL_PACKET			20

#define DUMB						0
#define PERTURBOBSERVE				1
#define INCREMENTALCONDUCTANCE		2
#define CONSTANTVOLTAGE				3

#define CURVETRACE_STEP_SIZE	PWM_TIMEBASE / 128
#define PO_TRACK_STEP_SIZE		PWM_TIMEBASE / 1024
#define INC_TRACK_STEP_SIZE		PWM_TIMEBASE / 1024
#define CV_TRACK_STEP_SIZE		PWM_TIMEBASE / 1024

/******************************************************************************/
/******************************************************************************/
/*Functions*/

void InitTracker(void);
void TrackerApplyMaxDac(void);
void TrackerStateMachine(void);
void ControlVoltages(void);
void CurveTrace(uint8 alg);
void Track(uint8 level);
void PerturbObserve(uint8 c);
void IncrementalConductance(uint8 c);
void ShutDownConverter(int state);
void RestartConverter(int state);
void FlashLEDCode(int state);
uint16 calculateSynchDutyCycle(void);
uint16 guessMPP(void);
uint8 setSynchronousDutyCycle(uint8 forceSynchDisable);
void PreloadForSweep(uint8 c);
void SweepArray(uint8 c);
void UpdateFloatParams(void);
void UpdateCvDac(void);
inline int32 LimitDac(int32 dac);
void SetDacValues(uint16 ch0, uint16 ch1);
#endif
