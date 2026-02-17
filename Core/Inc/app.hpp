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

#define FLASH_CFG_NUM_SECTOR 256// 1мб //SPIF_SECTOR_SIZE 0x1000 (4096)
#define FLASH_CFG_START_SECTOR 0



#endif /* INC_APP_HPP_ */
