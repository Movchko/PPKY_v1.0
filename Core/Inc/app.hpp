/*
 * app.hpp
 *
 *  Created on: Nov 20, 2025
 *      Author: 79099
 */

#ifndef INC_APP_HPP_
#define INC_APP_HPP_

#include "main.h"

/*
 * 6 кб имена зон
 *
 * 256 байт конфиг одного вирт устройства
 * 32 вирт устрйоства на 1 мку 8192 байт
 * 32мку на 1 линии
 */

#define FLASH_CFG_START_SECTOR 0
/* Секторов для конфигурации: header(8) + PPKYCfg(~40KB) ≈ 10 секторов, берём 12 с запасом */
#define FLASH_CFG_SECTORS_USED  12
#define FLASH_CFG_NUM_SECTOR    256  /* всего зарезервировано (1 МБ), стираем только FLASH_CFG_SECTORS_USED */

/** Заполнить PPKYConfig картой МКУ по шаблону (ID ППКУ сохраняется).
 *  9 МКУ: 6 МКУ-игнитер (1 пускатель + 1 ДПТ), 3 МКУ-ТС (только ДПТ).
 *  H_adr 1..9, зоны 1..3 (по 1 МКУ_ТС и 2 МКУ_игнитер на зону). */
void FillConfigTemplate(void);

#endif /* INC_APP_HPP_ */
