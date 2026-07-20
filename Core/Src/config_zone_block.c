#include "config_zone_block.h"
#include "device_config.h"

extern PPKYCfg PPKYConfig;

uint8_t PPKY_ZoneLaunchBlockedByCanZone(uint8_t zone_can)
{
	if (zone_can == 0u) {
		return 0u;
	}
	return PPKY_ZoneBlockGet(PPKYConfig.zone_block, (uint8_t)(zone_can - 1u));
}

uint8_t PPKY_ZoneBlockGet(const uint8_t *zone_block, uint8_t zone_idx)
{
	if (zone_block == NULL || zone_idx >= ZONE_NUMBER) {
		return 0u;
	}
	return (uint8_t)((zone_block[zone_idx / 8u] >> (zone_idx % 8u)) & 1u);
}

void PPKY_ZoneBlockSet(uint8_t *zone_block, uint8_t zone_idx, uint8_t blocked)
{
	if (zone_block == NULL || zone_idx >= ZONE_NUMBER) {
		return;
	}
	const uint8_t mask = (uint8_t)(1u << (zone_idx % 8u));
	if (blocked != 0u) {
		zone_block[zone_idx / 8u] |= mask;
	} else {
		zone_block[zone_idx / 8u] &= (uint8_t)(~mask);
	}
}
