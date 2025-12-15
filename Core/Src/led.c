/*
 * led.c
 *
 *  Created on: Dec 4, 2025
 *      Author: 79099
 */


#include "main.h"

extern I2C_HandleTypeDef hi2c1;

void LedInit() {
	  HAL_StatusTypeDef st = HAL_ERROR;
	  uint8_t led = 0b1111;
	  st = HAL_I2C_Mem_Write(&hi2c1, 0xC0, 0, I2C_MEMADD_SIZE_8BIT, &led, sizeof(led), 20); // Задаём режим OSC = normal
	  led = 0xff;
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
}

void LedSet(uint8_t ch, uint8_t st) {

}
