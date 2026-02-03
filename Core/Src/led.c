/*
 * led.c
 *
 *  Created on: Dec 4, 2025
 *      Author: 79099
 */


#include "main.h"

extern I2C_HandleTypeDef hi2c1;

#define NUM_LED 15
#define LED_ON 1
#define LED_OFF 0

uint8_t cur_led_state[NUM_LED];

void LedInit() {
	  HAL_StatusTypeDef st = HAL_ERROR;
	  uint8_t led = 0b1111;
	  st = HAL_I2C_Mem_Write(&hi2c1, 0xC0, 0, I2C_MEMADD_SIZE_8BIT, &led, sizeof(led), 20); // Задаём режим OSC = normal
	  //TODO обозначить ошибку, еслит нет связи
	  led =  0xff;
	  for(uint8_t i = 2; i <= 0x10; i++) {
		  st = HAL_I2C_Mem_Write(&hi2c1, 0xC0, i, I2C_MEMADD_SIZE_8BIT, &led, sizeof(led), 20); // включаем максимальную подстветку на каждом канале
		  HAL_Delay(1);
	  }
	  // зажигаем все светодиоды на 500мс
	  led = 0xFF;
	  st = HAL_I2C_Mem_Write(&hi2c1, 0xC0, 0x14, I2C_MEMADD_SIZE_8BIT, &led, sizeof(led), 20);
	  st = HAL_I2C_Mem_Write(&hi2c1, 0xC0, 0x15, I2C_MEMADD_SIZE_8BIT, &led, sizeof(led), 20);
	  st = HAL_I2C_Mem_Write(&hi2c1, 0xC0, 0x16, I2C_MEMADD_SIZE_8BIT, &led, sizeof(led), 20);
	  st = HAL_I2C_Mem_Write(&hi2c1, 0xC0, 0x17, I2C_MEMADD_SIZE_8BIT, &led, sizeof(led), 20);
	  HAL_Delay(500);
	  // выключаем все светодиоды
	  led = 0;
	  st = HAL_I2C_Mem_Write(&hi2c1, 0xC0, 0x14, I2C_MEMADD_SIZE_8BIT, &led, sizeof(led), 20);
	  st = HAL_I2C_Mem_Write(&hi2c1, 0xC0, 0x15, I2C_MEMADD_SIZE_8BIT, &led, sizeof(led), 20);
	  st = HAL_I2C_Mem_Write(&hi2c1, 0xC0, 0x16, I2C_MEMADD_SIZE_8BIT, &led, sizeof(led), 20);
	  st = HAL_I2C_Mem_Write(&hi2c1, 0xC0, 0x17, I2C_MEMADD_SIZE_8BIT, &led, sizeof(led), 20);

	  for(uint8_t i = 0; i < NUM_LED; i++) {
		  cur_led_state[i] = LED_OFF;
	  }

}

void LedSetAll(uint8_t power) {

	uint8_t led = 0xff;


	  for(uint8_t i = 2; i <= 0x10; i++) {
		  HAL_I2C_Mem_Write(&hi2c1, 0xC0, i, I2C_MEMADD_SIZE_8BIT, &power, sizeof(power), 20); // включаем максимальную подстветку на каждом канале
		  HAL_Delay(1);
	  }

	HAL_I2C_Mem_Write(&hi2c1, 0xC0, 0x14, I2C_MEMADD_SIZE_8BIT, &led, sizeof(led), 20);
	HAL_I2C_Mem_Write(&hi2c1, 0xC0, 0x15, I2C_MEMADD_SIZE_8BIT, &led, sizeof(led), 20);
	HAL_I2C_Mem_Write(&hi2c1, 0xC0, 0x16, I2C_MEMADD_SIZE_8BIT, &led, sizeof(led), 20);
	HAL_I2C_Mem_Write(&hi2c1, 0xC0, 0x17, I2C_MEMADD_SIZE_8BIT, &led, sizeof(led), 20);
	for(uint8_t i = 0; i < NUM_LED; i++) {
		cur_led_state[i] = LED_ON;
	}
}

void LedOffAll() {
	uint8_t led = 0;

	HAL_I2C_Mem_Write(&hi2c1, 0xC0, 0x14, I2C_MEMADD_SIZE_8BIT, &led, sizeof(led), 20);
	HAL_I2C_Mem_Write(&hi2c1, 0xC0, 0x15, I2C_MEMADD_SIZE_8BIT, &led, sizeof(led), 20);
	HAL_I2C_Mem_Write(&hi2c1, 0xC0, 0x16, I2C_MEMADD_SIZE_8BIT, &led, sizeof(led), 20);
	HAL_I2C_Mem_Write(&hi2c1, 0xC0, 0x17, I2C_MEMADD_SIZE_8BIT, &led, sizeof(led), 20);
	for(uint8_t i = 0; i < NUM_LED; i++) {
		cur_led_state[i] = LED_OFF;
	}
}


void LedSet(uint8_t led, uint8_t st) {
	cur_led_state[led] = st;
	uint8_t ch_group = led % 4;
	uint8_t val = 0;
	for(uint8_t i = 0; i < 4; i++) {
		val |=  cur_led_state[ch_group + i] << (i * 2);
	}

	HAL_I2C_Mem_Write(&hi2c1, 0xC0, 0x14 + ch_group, I2C_MEMADD_SIZE_8BIT, &val, sizeof(val), 20);
}
