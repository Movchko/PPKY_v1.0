/*
 * app.hpp
 *
 *  Created on: Nov 20, 2025
 *      Author: 79099
 */

#ifndef INC_APP_HPP_
#define INC_APP_HPP_

#include "main.h"

#define FLASH_CFG_START_SECTOR 0

#define FLASH_CFG_SECTORS_USED  24


void FillConfigTemplate(void);
void ReadSavedConfig(void);

/* Чтение содержимого BKP-регистра RTC с моментом последнего сохранения
 * (месяц/день/часы/минуты). Поля возвращаются в формате RTC (BCD),
 * как в HAL_RTC_GetDate / HAL_RTC_GetTime.
 * Любой из указателей может быть NULL, если часть данных не нужна. */
void PPKY_GetLastPowerOnDate(RTC_DateTypeDef *out_date, RTC_TimeTypeDef *out_time);

#endif /* INC_APP_HPP_ */
