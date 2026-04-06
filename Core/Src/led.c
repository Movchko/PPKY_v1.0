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
static uint8_t cur_led_power[NUM_LED];
static uint8_t hw_led_state[NUM_LED];
static uint8_t hw_led_power[NUM_LED];
static uint8_t led_sync_tick = 0u;

/* Счетчик неактивности кнопок для управления яркостью подсветки ENTER/ESC */
static uint16_t led_but_idle_counter = 0;
static uint8_t  led_but_is_bright = 0; /* 0 - базовая яркость, 1 - максимальная */

static uint8_t Led_PackGroupState(const uint8_t *state_arr, uint8_t group)
{
	uint8_t base = (uint8_t)(group * 4u);
	uint8_t val = 0u;
	for (uint8_t i = 0u; i < 4u; i++) {
		uint8_t idx = (uint8_t)(base + i);
		uint8_t st = (idx < NUM_LED) ? state_arr[idx] : LED_OFF;
		val |= ((st & 0x03u) << 1u) << (i * 2u);
	}
	return val;
}

static void Led_SyncToI2C(void)
{
	/* 1) Яркость по каналам */
	for (uint8_t i = 0u; i < NUM_LED; i++) {
		if (cur_led_power[i] != hw_led_power[i]) {
			uint8_t pwr = cur_led_power[i];
			HAL_I2C_Mem_Write(&hi2c1, 0xC0, (uint16_t)(2u + i), I2C_MEMADD_SIZE_8BIT, &pwr, sizeof(pwr), 20);
			hw_led_power[i] = pwr;
		}
	}

	/* 2) Состояния (регистры по 4 LED) */
	for (uint8_t group = 0u; group < 4u; group++) {
		uint8_t cur_val = Led_PackGroupState(cur_led_state, group);
		uint8_t hw_val  = Led_PackGroupState(hw_led_state, group);
		if (cur_val != hw_val) {
			HAL_I2C_Mem_Write(&hi2c1, 0xC0, (uint16_t)(0x14u + group), I2C_MEMADD_SIZE_8BIT, &cur_val, sizeof(cur_val), 20);
			uint8_t base = (uint8_t)(group * 4u);
			for (uint8_t i = 0u; i < 4u; i++) {
				uint8_t idx = (uint8_t)(base + i);
				if (idx < NUM_LED) {
					hw_led_state[idx] = cur_led_state[idx];
				}
			}
		}
	}
}

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
		  cur_led_power[i] = 0xFFu;
		  hw_led_state[i] = 0xFFu; /* форсируем первую синхронизацию */
		  hw_led_power[i] = 0xFFu; /* форсируем первую синхронизацию */
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
	  led_sync_tick = LED_I2C_SYNC_PERIOD_TICKS;


}

void Led_SetAll(uint8_t power) {
	for(uint8_t i = 0; i < NUM_LED; i++) {
		cur_led_state[i] = LED_ON;
		cur_led_power[i] = power;
	}
}

void Led_OffAll() {
	for(uint8_t i = 0; i < NUM_LED; i++) {
		cur_led_state[i] = LED_OFF;
	}
}


void Led_Set(uint8_t led, uint8_t st) {
	if (led >= NUM_LED) {
		return;
	}
    // st – только 2 бита
    st &= 0x03;
    if (cur_led_state[led] == st) {
        /* Состояние не изменилось */
        return;
    }
    cur_led_state[led] = st;
}

void Led_Snake(uint8_t state) {
	const uint8_t seq[] = {
		LED_BUT_ENTER_UP, LED_FIRE, LED_BUT_ESC_DW, LED_AUTO_OFF, LED_POWER,
		LED_NORM, LED_START, LED_STOP, LED_ERR, LED_BUT_START_ALL,
		LED_STR_START_ALL, LED_STR_STOP, LED_BUT_STOP, LED_BUT_START_SP, LED_STR_START_SP
	};
	for (uint8_t i = 0u; i < (uint8_t)(sizeof(seq) / sizeof(seq[0])); i++) {
		Led_Set(seq[i], state);
		Led_SyncToI2C();
		HAL_Delay(50);
	}

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

	/* Подсветку ENTER/ESC делаем активной только от нажатий ENTER/ESC.
	 * Иначе любые кнопки (например, ПУСК Общий) будут переключать яркость и
	 * дергать I2C слишком часто. */
	ButtonState st_enter = Button_GetState(BUT_ENTER);
	ButtonState st_esc   = Button_GetState(BUT_ESC);
	bool any_pressed = ((st_enter == ButtonStatePress) || (st_enter == ButtonStateLongPress) ||
	                     (st_esc == ButtonStatePress)   || (st_esc == ButtonStateLongPress));

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

	/* Периодическая синхронизация состояния/яркости в драйвер LED по I2C */
	if (led_sync_tick < LED_I2C_SYNC_PERIOD_TICKS) {
		led_sync_tick++;
	}
	if (led_sync_tick >= LED_I2C_SYNC_PERIOD_TICKS) {
		led_sync_tick = 0u;
		Led_SyncToI2C();
	}
}

void Led_SetBrightness(uint8_t led, uint8_t power) {
	if (led >= NUM_LED) {
		return;
	}
	cur_led_power[led] = power;
}


