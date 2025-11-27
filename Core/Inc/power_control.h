/*
 * power_control.hpp
 *
 *  Created on: Nov 20, 2025
 *      Author: 79099
 */

#ifndef INC_POWER_CONTROL_H_
#define INC_POWER_CONTROL_H_

#include <stdint.h>
#include <string.h>

enum PControlState {
	PControl_Init,
	PControl_Idle,
	PControl_Normal,
	PControl_Fault
};

class PControl {
	uint8_t Num;
	PControlState state;

	float current;
	uint8_t status;

//	uint8_t	(*PControlGetST)(uint8_t ch);
//	uint32_t (*PControlGetADC)(uint8_t ch);
//	void (*PControlSetOut)(uint8_t ch, uint8_t out);


public:
	PControl(uint8_t ch);
	void PControlInit(uint8_t (*PControlGetSTCB)(uint8_t),
					  uint32_t (*PControlGetADCCB)(uint8_t),
					  void (*PControlSetOutCB)(uint8_t, uint8_t));
	void Process();

	PControlState GetState() { return state;}
	float GetCurrent() { return current;}

	uint8_t	(*PControlGetST)(uint8_t ch);
	uint32_t (*PControlGetADC)(uint8_t ch);
	void (*PControlSetOut)(uint8_t ch, uint8_t out);


};


#endif /* INC_POWER_CONTROL_H_ */
