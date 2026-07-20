#ifndef INC_CONFIG_ZONE_BLOCK_H_
#define INC_CONFIG_ZONE_BLOCK_H_

#include <stdint.h>
#include "device_config.h"

#ifdef __cplusplus
extern "C" {
#endif

uint8_t PPKY_ZoneBlockGet(const uint8_t *zone_block, uint8_t zone_idx);
void PPKY_ZoneBlockSet(uint8_t *zone_block, uint8_t zone_idx, uint8_t blocked);
/* zone_can: 0 — служебная; 1..N → индекс zone_can-1 в zone_block / zone_name. */
uint8_t PPKY_ZoneLaunchBlockedByCanZone(uint8_t zone_can);

#ifdef __cplusplus
}
#endif

#endif /* INC_CONFIG_ZONE_BLOCK_H_ */
