/*
 * beeper.c
 *
 *  Created on: 2025
 *      Author: 79099
 */

#include "beeper.h"
#include "main.h"

/***********************************************************************************************************/
/* Внутренние типы и переменные */
/***********************************************************************************************************/

typedef enum
{
	BEEPER_STATE_IDLE = 0,              // Простой
	BEEPER_STATE_SHORT_BEEP,           // Одно короткое пищание
	BEEPER_STATE_DOUBLE_SHORT_BEEP,    // Два коротких пищания
	BEEPER_STATE_LONG_BEEP,            // Длинное пищание
	BEEPER_STATE_CONTINUOUS,            // Постоянное пищание
	BEEPER_STATE_FIRE_ALARM             // Пожар: короткие включения с паузами
} BeeperState_t;

/* Пожар: ~200 мс звук, ~300 мс тишина (шаг 10 мс в Beeper_Process) */
#define BEEPER_FIRE_ON_TICKS   20u
#define BEEPER_FIRE_OFF_TICKS  30u

static BeeperState_t beeper_state = BEEPER_STATE_IDLE;
static uint16_t beeper_counter = 0;
static uint8_t beep_phase = 0;  // Фаза для двойного пищания (0 - первое, 1 - пауза, 2 - второе)
static uint8_t fire_alarm_sound = 1u; /* 1 = фаза «звук», 0 = пауза */

static uint8_t beep_sound = 1;

/***********************************************************************************************************/
/* Внутренние функции */
/***********************************************************************************************************/

/**
 * @brief Включение звука
 */
static void Beeper_On(void)
{
	if(beep_sound == 0)
		return;

	HAL_GPIO_WritePin(SOUND_GPIO_Port, SOUND_Pin, GPIO_PIN_SET);
}

/**
 * @brief Выключение звука
 */
static void Beeper_Off(void)
{
	HAL_GPIO_WritePin(SOUND_GPIO_Port, SOUND_Pin, GPIO_PIN_RESET);
}

/***********************************************************************************************************/
/* Публичные функции */
/***********************************************************************************************************/

/**
 * @brief Инициализация пищалки
 */
void Beeper_Init(void)
{
	beeper_state = BEEPER_STATE_IDLE;
	beeper_counter = 0;
	beep_phase = 0;
	Beeper_Off();
}

/**
 * @brief Одно короткое пищание (10мс)
 */
void Beeper_ShortBeep(void)
{
	beeper_state = BEEPER_STATE_SHORT_BEEP;
	beeper_counter = BEEPER_SHORT_BEEP_DURATION;
	beep_phase = 0;
	Beeper_On();
}

/**
 * @brief Два коротких пищания с паузой между ними
 */
void Beeper_DoubleShortBeep(void)
{
	beeper_state = BEEPER_STATE_DOUBLE_SHORT_BEEP;
	beeper_counter = BEEPER_SHORT_BEEP_DURATION;
	beep_phase = 0;  // Начинаем с первого пищания
	Beeper_On();
}

/**
 * @brief Длинное пищание (100мс)
 */
void Beeper_LongBeep(void)
{
	beeper_state = BEEPER_STATE_LONG_BEEP;
	beeper_counter = BEEPER_LONG_BEEP_DURATION;
	beep_phase = 0;
	Beeper_On();
}

/**
 * @brief Включить постоянное пищание
 */
void Beeper_ContinuousOn(void)
{
	beeper_state = BEEPER_STATE_CONTINUOUS;
	beeper_counter = 0;
	beep_phase = 0;
	Beeper_On();
}

void Beeper_FireAlarmOn(void)
{
	beeper_state = BEEPER_STATE_FIRE_ALARM;
	fire_alarm_sound = 1u;
	beeper_counter = BEEPER_FIRE_ON_TICKS;
	Beeper_On();
}

void Beeper_FireAlarmOff(void)
{
	if (beeper_state == BEEPER_STATE_FIRE_ALARM) {
		beeper_state = BEEPER_STATE_IDLE;
		Beeper_Off();
	}
}

/**
 * @brief Выключить постоянное пищание
 */
void Beeper_ContinuousOff(void)
{
	if (beeper_state == BEEPER_STATE_CONTINUOUS)
	{
		beeper_state = BEEPER_STATE_IDLE;
		Beeper_Off();
	}
}

/**
 * @brief Переключить состояние постоянного пищания
 */
void Beeper_ContinuousToggle(void)
{
	if (beeper_state == BEEPER_STATE_CONTINUOUS)
	{
		Beeper_ContinuousOff();
	}
	else
	{
		Beeper_ContinuousOn();
	}
}

/**
 * @brief Функция обработки состояния пищалки (вызывать каждые 10мс)
 * @note Должна вызываться из таймера или основного цикла с периодом 10мс
 */
void Beeper_Process(void)
{


	switch (beeper_state)
	{
		case BEEPER_STATE_IDLE:
			// Ничего не делаем
			break;

		case BEEPER_STATE_SHORT_BEEP:
			// Одно короткое пищание
			if (beeper_counter > 0)
			{
				beeper_counter--;
			}
			else
			{
				// Пищание завершено
				Beeper_Off();
				beeper_state = BEEPER_STATE_IDLE;
			}
			break;

		case BEEPER_STATE_DOUBLE_SHORT_BEEP:
			// Два коротких пищания
			if (beeper_counter > 0)
			{
				beeper_counter--;
			}
			else
			{
				// Текущая фаза завершена
				if (beep_phase == 0)
				{
					// Первое пищание завершено, начинаем паузу
					Beeper_Off();
					beep_phase = 1;
					beeper_counter = BEEPER_PAUSE_DURATION;
				}
				else if (beep_phase == 1)
				{
					// Пауза завершена, начинаем второе пищание
					Beeper_On();
					beep_phase = 2;
					beeper_counter = BEEPER_SHORT_BEEP_DURATION;
				}
				else
				{
					// Второе пищание завершено
					Beeper_Off();
					beeper_state = BEEPER_STATE_IDLE;
					beep_phase = 0;
				}
			}
			break;

		case BEEPER_STATE_LONG_BEEP:
			// Длинное пищание
			if (beeper_counter > 0)
			{
				beeper_counter--;
			}
			else
			{
				// Пищание завершено
				Beeper_Off();
				beeper_state = BEEPER_STATE_IDLE;
			}
			break;

		case BEEPER_STATE_CONTINUOUS:
			// Постоянное пищание - ничего не делаем, звук уже включен
			break;

		case BEEPER_STATE_FIRE_ALARM:
			if (beeper_counter > 0u) {
				beeper_counter--;
			} else {
				if (fire_alarm_sound) {
					Beeper_Off();
					fire_alarm_sound = 0u;
					beeper_counter = BEEPER_FIRE_OFF_TICKS;
				} else {
					Beeper_On();
					fire_alarm_sound = 1u;
					beeper_counter = BEEPER_FIRE_ON_TICKS;
				}
			}
			break;

		default:
			// Неизвестное состояние - переходим в IDLE
			beeper_state = BEEPER_STATE_IDLE;
			Beeper_Off();
			break;
	}
}

void Beeper_SoundOnOff(bool soundOn) {
	beep_sound = soundOn;
}
