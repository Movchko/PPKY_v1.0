/*
 * button.h
 *
 *  Created on: Dec 22, 2025
 *      Author: 79099
 */

#ifndef INC_BUTTON_H_
#define INC_BUTTON_H_

#include "main.h"

#define NUM_BUTTON 	7

#define BUT_ENTER 	0
#define BUT_UP 		1
#define BUT_DOWN	2
#define BUT_ESC		3
#define BUT_FORCE	4
#define BUT_STOP	5
#define BUT_FIRE 	6

#define LONG_PRESS_COUNT 20 // 2s. при таймере 10гц.
#define SHORT_PRESS_COUNT 1 // 100ms. при таймере 10гц.

#ifdef __cplusplus
extern "C" {
#endif

typedef enum ButtonState {
	ButtonStateReset = 0,
	ButtonStatePress = 1,
	ButtonStateLongPress = 2,
	ButtonStateError = 3
}ButtonState;

struct Button {
	ButtonState state;
	uint8_t 	press_counter;
	uint8_t		ispress;
};



void Button_Init();
void Button_Process(); // таймер 10гц
ButtonState Button_GetState(uint8_t but);
void Button_ReadPin(); // чтение состояний

#ifdef __cplusplus
}
#endif

#endif /* INC_BUTTON_H_ */
