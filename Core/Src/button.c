/*
 * button.c
 *
 *  Created on: Dec 4, 2025
 *      Author: 79099
 */

#include "button.h"
#include "beeper.h"
#include "fire.h"

struct Button Buttons[NUM_BUTTON];
extern I2C_HandleTypeDef hi2c1;
/* Интервал между попытками восстановления I2C (в тиках Button_Process, ~10 мс). */
#define BUTTON_I2C_RECOVERY_RETRY_TICKS 10u

static uint8_t s_btn_i2c_recovery_tick = BUTTON_I2C_RECOVERY_RETRY_TICKS;

static void Button_SetAllError(void)
{
	for(uint8_t i = 0; i < NUM_BUTTON; i++) {
		Buttons[i].state = ButtonStateError;
	}
}

static void Button_ResetStates(void)
{
	for (uint8_t i = 0; i < NUM_BUTTON; i++) {
		Buttons[i].state = ButtonStateReset;
		Buttons[i].press_counter = 0;
		Buttons[i].ispress = 0;
	}
}

static uint8_t Button_ReinitI2CDriver(void)
{
	if (HAL_I2C_DeInit(&hi2c1) != HAL_OK) {
		return 0u;
	}
	if (HAL_I2C_Init(&hi2c1) != HAL_OK) {
		return 0u;
	}
	if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK) {
		return 0u;
	}
	if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK) {
		return 0u;
	}
	return 1u;
}

__attribute__((weak)) void Button_ReinitReaderChip(void)
{
	/* Задел под будущую программную реинициализацию внешней кнопочной микросхемы. */
}

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
		if (s_btn_i2c_recovery_tick < BUTTON_I2C_RECOVERY_RETRY_TICKS) {
			s_btn_i2c_recovery_tick++;
			return;
		}
		s_btn_i2c_recovery_tick = 0u;
		if (Button_ReinitI2CDriver()) {
			Button_ReinitReaderChip();
			Button_ResetStates();
		} else {
			Button_SetAllError();
		}
		return;
	}
	s_btn_i2c_recovery_tick = BUTTON_I2C_RECOVERY_RETRY_TICKS;

	for(uint8_t i = 0; i < NUM_BUTTON; i++) {
		if(Buttons[i].ispress == 0) {
			Buttons[i].state = ButtonStateReset;
			Buttons[i].press_counter = 0;
		} else {

			if((Buttons[i].press_counter >= LONG_PRESS_COUNT) && (Buttons[i].state == ButtonStatePress))
				Buttons[i].state = ButtonStateLongPress;
			if((Buttons[i].press_counter >= SHORT_PRESS_COUNT) && (Buttons[i].state == ButtonStateReset)) {
				Buttons[i].state = ButtonStatePress;
				/* Подтверждаем нажатие, не прерывая дежурные фоновые паттерны. */
				Beeper_ButtonAcknowledge();

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
		Button_SetAllError();
		return;
	}
	/* Логические номера BUT_* соответствуют битам расширителя I2C */
	Buttons[BUT_ENTER].ispress 	= (but >> BUT_ENTER) & 0x1;
	Buttons[BUT_UP].ispress 	= (but >> BUT_UP) & 0x1;
	Buttons[BUT_DOWN].ispress	= (but >> BUT_DOWN) & 0x1;
	Buttons[BUT_ESC].ispress 	= (but >> BUT_ESC) & 0x1;
	Buttons[BUT_FORCE].ispress  = HAL_GPIO_ReadPin(BT_FORCE_ACT_GPIO_Port, BT_FORCE_ACT_Pin);
	Buttons[BUT_STOP].ispress  = HAL_GPIO_ReadPin(BT_STOP_GPIO_Port, BT_STOP_Pin);
	Buttons[BUT_FIRE].ispress  = HAL_GPIO_ReadPin(BT_FIRE_GPIO_Port, BT_FIRE_Pin);
}
