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

#define BEEPER_SHORT_BEEP_DURATION     1   // Короткое пищание: 10мс (1 * 10мс)
#define BEEPER_LONG_BEEP_DURATION      10  // Длинное пищание: 100мс (10 * 10мс)
#define BEEPER_PAUSE_DURATION          1   // Пауза между пищаниями: 10мс (1 * 10мс)

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

/**
 * @brief Включить постоянное пищание
 */
void Beeper_ContinuousOn(void);

/**
 * @brief Выключить постоянное пищание
 */
void Beeper_ContinuousOff(void);

/**
 * @brief Переключить состояние постоянного пищания
 */
void Beeper_ContinuousToggle(void);

/**
 * @brief Функция обработки состояния пищалки (вызывать каждые 10мс)
 * @note Должна вызываться из таймера или основного цикла с периодом 10мс
 */
void Beeper_Process(void);

#ifdef __cplusplus
}
#endif

#endif /* INC_BEEPER_H_ */