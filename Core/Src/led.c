/*
 * led.c
 *
 *  Created on: Dec 4, 2025
 *      Author: 79099
 */


#include "main.h"
#include "led.h"
#include "button.h"

extern I2C_HandleTypeDef hi2c1;

#define NUM_LED 15
#define LED_ON 1
#define LED_OFF 0

uint8_t cur_led_state[NUM_LED];

/* Счетчик неактивности кнопок для управления яркостью подсветки ENTER/ESC */
static uint16_t led_but_idle_counter = 0;
static uint8_t  led_but_is_bright = 0; /* 0 - базовая яркость, 1 - максимальная */

void Led_Init() {
	  HAL_StatusTypeDef st = HAL_ERROR;
	  uint8_t led = 0b1111;
	  st = HAL_I2C_Mem_Write(&hi2c1, 0xC0, 0, I2C_MEMADD_SIZE_8BIT, &led, sizeof(led), 20); // Задаём режим OSC = normal
	  //TODO обозначить ошибку, еслит нет связи
	  led =  0xff;
	  for(uint8_t i = 2; i <= 0x10; i++) {
		  st = HAL_I2C_Mem_Write(&hi2c1, 0xC0, i, I2C_MEMADD_SIZE_8BIT, &led, sizeof(led), 20); // включаем максимальную подстветку на каждом канале
		  HAL_Delay(1);
	  }
	  /*
	  // зажигаем все светодиоды на 500мс
	  led = 0xFF;
	  st = HAL_I2C_Mem_Write(&hi2c1, 0xC0, 0x14, I2C_MEMADD_SIZE_8BIT, &led, sizeof(led), 20);
	  st = HAL_I2C_Mem_Write(&hi2c1, 0xC0, 0x15, I2C_MEMADD_SIZE_8BIT, &led, sizeof(led), 20);
	  st = HAL_I2C_Mem_Write(&hi2c1, 0xC0, 0x16, I2C_MEMADD_SIZE_8BIT, &led, sizeof(led), 20);
	  st = HAL_I2C_Mem_Write(&hi2c1, 0xC0, 0x17, I2C_MEMADD_SIZE_8BIT, &led, sizeof(led), 20);
	  HAL_Delay(100);
	  */
	  // выключаем все светодиоды
	  led = 0;
	  st = HAL_I2C_Mem_Write(&hi2c1, 0xC0, 0x14, I2C_MEMADD_SIZE_8BIT, &led, sizeof(led), 20);
	  st = HAL_I2C_Mem_Write(&hi2c1, 0xC0, 0x15, I2C_MEMADD_SIZE_8BIT, &led, sizeof(led), 20);
	  st = HAL_I2C_Mem_Write(&hi2c1, 0xC0, 0x16, I2C_MEMADD_SIZE_8BIT, &led, sizeof(led), 20);
	  st = HAL_I2C_Mem_Write(&hi2c1, 0xC0, 0x17, I2C_MEMADD_SIZE_8BIT, &led, sizeof(led), 20);
	  HAL_Delay(100);

	  Led_Snake(1);
	  Led_Snake(0);

	  for(uint8_t i = 0; i < NUM_LED; i++) {
		  cur_led_state[i] = LED_OFF;
		  Led_SetBrightness(i, 0xFF);
	  }

	  /* Подсветка кнопок ENTER/ESC по умолчанию включена с небольшой яркостью */
	  Led_Set(LED_BUT_ENTER_UP, LED_ON);
	  Led_Set(LED_BUT_ESC_DW,  LED_ON);
	  Led_SetBrightness(LED_BUT_ENTER_UP, LED_BUT_DIM_BRIGHTNESS);
	  Led_SetBrightness(LED_BUT_ESC_DW,  LED_BUT_DIM_BRIGHTNESS);

	  Led_Set(LED_POWER, LED_ON);
	  Led_Set(LED_NORM,  LED_ON);
	  Led_SetBrightness(LED_POWER, LED_BUT_DIM_BRIGHTNESS);
	  Led_SetBrightness(LED_NORM,  LED_BUT_DIM_BRIGHTNESS);

	  led_but_is_bright = 0;


}

void Led_SetAll(uint8_t power) {

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

void Led_OffAll() {
	uint8_t led = 0;

	HAL_I2C_Mem_Write(&hi2c1, 0xC0, 0x14, I2C_MEMADD_SIZE_8BIT, &led, sizeof(led), 20);
	HAL_I2C_Mem_Write(&hi2c1, 0xC0, 0x15, I2C_MEMADD_SIZE_8BIT, &led, sizeof(led), 20);
	HAL_I2C_Mem_Write(&hi2c1, 0xC0, 0x16, I2C_MEMADD_SIZE_8BIT, &led, sizeof(led), 20);
	HAL_I2C_Mem_Write(&hi2c1, 0xC0, 0x17, I2C_MEMADD_SIZE_8BIT, &led, sizeof(led), 20);
	for(uint8_t i = 0; i < NUM_LED; i++) {
		cur_led_state[i] = LED_OFF;
	}
}


void Led_Set(uint8_t led, uint8_t st) {
    // st – только 2 бита
    st &= 0x03;
    if (cur_led_state[led] == st) {
        /* Состояние не изменилось – нет смысла дёргать I2C */
        return;
    }
    cur_led_state[led] = st;

    uint8_t group = led / 4;          // номер группы (0..3)
    uint8_t base  = group * 4;        // базовый индекс в cur_led_state для этой группы

    uint8_t val = 0;
    for (uint8_t i = 0; i < 4; i++) {
        // упаковываем 4 светодиода группы в байт: по 2 бита на светодиод
        val |= ((cur_led_state[base + i] & 0x03) << 1) << (i * 2);
    }

    //Led_SetBrightness(LED_BUT_ENTER_UP, 10 + 10*led);
    //Led_SetBrightness(LED_BUT_ESC_DW, 10 + 10*led);

    HAL_I2C_Mem_Write(&hi2c1,
                      0xC0,
                      0x14 + group,            // адрес регистра для группы
                      I2C_MEMADD_SIZE_8BIT,
                      &val,
                      sizeof(val),
                      20);



}

void Led_Snake(uint8_t state) {
	uint32_t delay = 50;
	Led_Set(LED_BUT_ENTER_UP, state); 	HAL_Delay(delay);
	Led_Set(LED_FIRE, state); 			HAL_Delay(delay);
	Led_Set(LED_BUT_ESC_DW, state);		HAL_Delay(delay);
	Led_Set(LED_AUTO_OFF, state);		HAL_Delay(delay);
	Led_Set(LED_POWER, state);		HAL_Delay(delay);
	Led_Set(LED_NORM, state);		HAL_Delay(delay);
	Led_Set(LED_START, state);		HAL_Delay(delay);
	Led_Set(LED_STOP, state);		HAL_Delay(delay);
	Led_Set(LED_ERR, state);					HAL_Delay(delay);
	Led_Set(LED_BUT_START_ALL, state);		HAL_Delay(delay);
	Led_Set(LED_STR_START_ALL, state);		HAL_Delay(delay);
	Led_Set(LED_STR_STOP, state);		HAL_Delay(delay);
	Led_Set(LED_BUT_STOP, state);		HAL_Delay(delay);
	Led_Set(LED_BUT_START_SP, state);		HAL_Delay(delay);
	Led_Set(LED_STR_START_SP, state);		HAL_Delay(delay);

/*
	for(uint8_t i = 0; i < NUM_LED; i++) {
		LedSet(i, state);
		HAL_Delay(delay);
	}
*/
}

void Led_TestToogle() {
	for(uint8_t i = 2; i < (NUM_LED - 2); i++) {
		if(cur_led_state[i]) {
			Led_Set(i, 0);
			if(i < (NUM_LED - 1 - 2))
				Led_Set(i + 1, 1);
			else
				Led_Set(2, 1);
			return;
		}
	}
	Led_Set(2, 1);
}

void Led_Process() {
	/* Подсветка кнопок ENTER/ESC:
	 * - по умолчанию горит с мощностью LED_BUT_DIM_BRIGHTNESS
	 * - при нажатии любой кнопки – максимум LED_BUT_MAX_BRIGHTNESS
	 * - если LED_BUT_IDLE_TIMEOUT_TICKS (3 секунды при шаге 10 мс) нет нажатий –
	 *   снова небольшая яркость
	 */

	bool any_pressed = false;

	for (uint8_t i = 0; i < NUM_BUTTON; i++) {
		ButtonState st = Button_GetState(i);
		if ((st == ButtonStatePress) || (st == ButtonStateLongPress)) {
			any_pressed = true;
			break;
		}
	}

	if (any_pressed) {
		/* Есть нажатие – сразу делаем максимум и обнуляем таймер */
		led_but_idle_counter = 0;
		if (!led_but_is_bright) {
			led_but_is_bright = 1;
			Led_SetBrightness(LED_BUT_ENTER_UP, LED_BUT_MAX_BRIGHTNESS);
			Led_SetBrightness(LED_BUT_ESC_DW,  LED_BUT_MAX_BRIGHTNESS);
			Led_SetBrightness(LED_POWER, LED_BUT_MAX_BRIGHTNESS);
			Led_SetBrightness(LED_NORM,  LED_BUT_MAX_BRIGHTNESS);
		}
	} else {
		/* Нет нажатий – считаем время простоя */
		if (led_but_idle_counter < LED_BUT_IDLE_TIMEOUT_TICKS) {
			led_but_idle_counter++;
			if ((led_but_idle_counter >= LED_BUT_IDLE_TIMEOUT_TICKS) && led_but_is_bright) {
				/* По истечении таймаута возвращаемся к небольшой яркости */
				led_but_is_bright = 0;
				Led_SetBrightness(LED_BUT_ENTER_UP, LED_BUT_DIM_BRIGHTNESS);
				Led_SetBrightness(LED_BUT_ESC_DW,  LED_BUT_DIM_BRIGHTNESS);
				Led_SetBrightness(LED_POWER, LED_BUT_DIM_BRIGHTNESS);
				Led_SetBrightness(LED_NORM,  LED_BUT_DIM_BRIGHTNESS);
			}
		}
	}
}

void Led_SetBrightness(uint8_t led, uint8_t power) {

    HAL_I2C_Mem_Write(&hi2c1,
                      0xC0,
                      2 + led,
                      I2C_MEMADD_SIZE_8BIT,
                      &power,
                      sizeof(power),
                      20);
}


