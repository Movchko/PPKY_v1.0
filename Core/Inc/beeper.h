/*
 * beeper.h
 *
 *  Created on: 2025
 *      Author: 79099
 */

#ifndef INC_BEEPER_H_
#define INC_BEEPER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdbool.h>
#include <stdint.h>

/***********************************************************************************************************/
/* Константы длительностей (в единицах вызова process, т.е. в 10мс) */
/***********************************************************************************************************/

#define BEEPER_SHORT_BEEP_DURATION     10   // Короткое пищание: 100мс (10 * 10мс)
#define BEEPER_LONG_BEEP_DURATION      130  // Длинное пищание: 1300мс (130 * 10мс)
#define BEEPER_PAUSE_DURATION          20   // Пауза между пищаниями: 200мс (20 * 10мс)

/* Согласованные паттерны звуковой индикации (ms) */
#define BEEPER_PATTERN_FAULT_ON_MS        10u
#define BEEPER_PATTERN_FAULT_OFF_MS       50u
#define BEEPER_PATTERN_FAULT_PULSES       2u
#define BEEPER_PATTERN_FAULT_REPEAT_MS    10000u

#define BEEPER_PATTERN_START_ON_MS        200u
#define BEEPER_PATTERN_START_OFF_MS       200u
#define BEEPER_PATTERN_START_PULSES       1u
/* Тушение: короткие частые пищания */
#define BEEPER_PATTERN_START_REPEAT_MS    100u

#define BEEPER_PATTERN_FIRE_ON_MS         70u
#define BEEPER_PATTERN_FIRE_OFF_MS        50u
#define BEEPER_PATTERN_FIRE_PULSES        1u
#define BEEPER_PATTERN_FIRE_REPEAT_MS     10000u

/***********************************************************************************************************/
/* Прототипы функций */
/***********************************************************************************************************/

/**
 * @brief Инициализация пищалки
 */
void Beeper_Init(void);

/**
 * @brief Одно короткое пищание (10мс)
 */
void Beeper_ShortBeep(void);

/**
 * @brief Два коротких пищания с паузой между ними
 */
void Beeper_DoubleShortBeep(void);

/**
 * @brief Длинное пищание (100мс)
 */
void Beeper_LongBeep(void);
void Beeper_LongBeep1300ms(void);

/**
 * @brief Включить постоянное пищание
 */
void Beeper_ContinuousOn(void);

/**
 * @brief Пожар: прерывистый сигнал (отдельный от непрерывного «пищит»).
 */
void Beeper_FireAlarmOn(void);

/**
 * @brief Выключить сигнал пожара (прерывистый).
 */
void Beeper_FireAlarmOff(void);

/**
 * @brief Выключить постоянное пищание
 */
void Beeper_ContinuousOff(void);
void Beeper_StopPattern(void);

/**
 * @brief Переключить состояние постоянного пищания
 */
void Beeper_ContinuousToggle(void);

/**
 * @brief Функция обработки состояния пищалки (вызывать каждые 10мс)
 * @note Должна вызываться из таймера или основного цикла с периодом 10мс
 */
void Beeper_Process(void);
void Beeper_PlayOneShotMs(uint16_t duration_ms);
void Beeper_StartPulseTrain(uint16_t pulse_on_ms, uint16_t pulse_off_ms, uint8_t pulses, uint16_t repeat_period_ms);
/* Короткий клик кнопки без потери фонового дежурного паттерна. */
void Beeper_ButtonAcknowledge(void);

/**
 * @brief Функция установки параметра звука ВКЛ/ВЫКЛ
 * Параметр сохраняемый в настройках
 */
void Beeper_SoundOnOff(bool soundOn);

#ifdef __cplusplus
}
#endif

#endif /* INC_BEEPER_H_ */
