/*
 * power_control.cpp
 *
 *  Created on: Nov 20, 2025
 *      Author: 79099
 */

#include "power_control.h"

PControl::PControl(uint8_t ch) {
	Num = ch;
	state = PControl_Init;
	current = 0;
	status = false;
}

void PControl::PControlInit(uint8_t (*PControlGetSTCB)(uint8_t),
					  uint32_t (*PControlGetADCCB)(uint8_t),
					  void (*PControlSetOutCB)(uint8_t, uint8_t)) {

	PControlGetST = PControlGetSTCB;
	PControlGetADC = PControlGetADCCB;
	PControlSetOut = PControlSetOutCB;

	PControlSetOut(Num, false);
	state = PControl_Idle;
}

void PControl::Process() {

	status = PControlGetST(Num);
	current = PControlGetADC(Num);

	switch(state) {
		case PControl_Init: break;
		case PControl_Idle: {

			PControlSetOut(Num, true);
			state = PControl_Normal;
		}break;
		case PControl_Normal: {

		}break;
		case PControl_Fault: {

		}break;
	}
}
