/*
 * button.c
 *
 *  Created on: Dec 4, 2025
 *      Author: 79099
 */

#include "button.h"
#include "beeper.h"

struct Button Buttons[NUM_BUTTON];
extern I2C_HandleTypeDef hi2c1;

void Button_Init() {
	for(uint8_t i = 0; i < NUM_BUTTON; i++) {
		Buttons[i].state = ButtonStateReset;
		Buttons[i].press_counter = 0;
		Buttons[i].ispress = 0;
	}
	HAL_StatusTypeDef st = HAL_ERROR;
	uint8_t but = 0xFF;
	st = HAL_I2C_Mem_Read(&hi2c1, 0x41<<1, 0x00, I2C_MEMADD_SIZE_8BIT, &but, sizeof(but), 10);
	if(st != HAL_OK) {
		for(uint8_t i = 0; i < NUM_BUTTON; i++) {
			Buttons[i].state = ButtonStateError;
		}
		return;
	}
}

void Button_Process() {
	Button_ReadPin();
	if(Buttons[0].state == ButtonStateError) {
		Beeper_DoubleShortBeep();
		return;
	}

	for(uint8_t i = 0; i < NUM_BUTTON; i++) {
		if(Buttons[i].ispress == 0) {
			Buttons[i].state = ButtonStateReset;
			Buttons[i].press_counter = 0;
		} else {

			if((Buttons[i].press_counter >= LONG_PRESS_COUNT) && (Buttons[i].state == ButtonStatePress))
				Buttons[i].state = ButtonStateLongPress;
			if((Buttons[i].press_counter >= SHORT_PRESS_COUNT) && (Buttons[i].state == ButtonStateReset)) {
				Buttons[i].state = ButtonStatePress;


				if((i == BUT_UP) || (i == BUT_DOWN) || (i == BUT_ESC))
					Beeper_ShortBeep();
				else if((i == BUT_ENTER))
					Beeper_DoubleShortBeep();
				else
					Beeper_LongBeep();

			}

			Buttons[i].press_counter++;
		}
	}
}

ButtonState Button_GetState(uint8_t but) {
	return Buttons[but].state;
}

void Button_ReadPin() {
	HAL_StatusTypeDef st = HAL_ERROR;
	uint8_t but = 0xFF;
	st = HAL_I2C_Mem_Read(&hi2c1, 0x41<<1, 0x00, I2C_MEMADD_SIZE_8BIT, &but, sizeof(but), 10);
	if(st != HAL_OK) {
		for(uint8_t i = 0; i < NUM_BUTTON; i++) {
			Buttons[i].state = ButtonStateError;
		}
		return;
	}
	Buttons[BUT_ENTER].ispress 	= (but>>0) & 0x1;
	Buttons[BUT_UP].ispress 	= (but>>1) & 0x1;
	Buttons[BUT_DOWN].ispress	= (but>>2) & 0x1;
	Buttons[BUT_ESC].ispress 	= (but>>3) & 0x1;
	Buttons[BUT_FORCE].ispress  = HAL_GPIO_ReadPin(BT_FORCE_ACT_GPIO_Port, BT_FORCE_ACT_Pin);
	Buttons[BUT_STOP].ispress  = HAL_GPIO_ReadPin(BT_STOP_GPIO_Port, BT_STOP_Pin);
	Buttons[BUT_FIRE].ispress  = HAL_GPIO_ReadPin(BT_FIRE_GPIO_Port, BT_FIRE_Pin);
}
