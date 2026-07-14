/*
 * event_log_catalog.c
 *
 * Таблица событий: уровень (критический / общий) и признак debug-only.
 * Менять наполнение и маршрутизацию — только здесь.
 */

#include "event_log_catalog.h"
#include <stddef.h>

#define EV(code, level, debug_only) \
	{ (uint16_t)(code), (level), (uint8_t)(debug_only) }

const EventLogDescriptor_t g_event_log_catalog[] = {
	EV(EVENT_LOG_MASTER_START,           EVENT_LOG_LEVEL_CRITICAL, 0),
	EV(EVENT_LOG_MASTER_STOP,            EVENT_LOG_LEVEL_CRITICAL, 0),
	EV(EVENT_LOG_SYSTEM_START_OK,        EVENT_LOG_LEVEL_CRITICAL, 0),
	EV(EVENT_LOG_DEVICE_MISSING,         EVENT_LOG_LEVEL_GENERAL,  0),
	EV(EVENT_LOG_DEVICE_FOUND,           EVENT_LOG_LEVEL_GENERAL,  0),
	EV(EVENT_LOG_CONFIG_MISMATCH,        EVENT_LOG_LEVEL_GENERAL,  0),
	EV(EVENT_LOG_TELEMETRY,              EVENT_LOG_LEVEL_GENERAL,  0),
	EV(EVENT_LOG_DEVICE_FAULT,           EVENT_LOG_LEVEL_CRITICAL, 0),
	EV(EVENT_LOG_FIRE_DETECTED,          EVENT_LOG_LEVEL_CRITICAL, 0),
	EV(EVENT_LOG_EXTINGUISH_START,       EVENT_LOG_LEVEL_CRITICAL, 0),
	EV(EVENT_LOG_EXTINGUISH_FORCE_STOP,  EVENT_LOG_LEVEL_CRITICAL, 0),
	EV(EVENT_LOG_EXTINGUISH_COMPLETE,    EVENT_LOG_LEVEL_CRITICAL, 0),
	EV(EVENT_LOG_EXTINGUISH_INCOMPLETE,  EVENT_LOG_LEVEL_CRITICAL, 0),
	EV(EVENT_LOG_PANEL_BUTTON,           EVENT_LOG_LEVEL_CRITICAL, 0),
};

const uint32_t g_event_log_catalog_count = sizeof(g_event_log_catalog) / sizeof(g_event_log_catalog[0]);

const EventLogDescriptor_t *EventLogCatalog_Find(uint16_t code)
{
	for (uint32_t i = 0; i < g_event_log_catalog_count; i++) {
		if (g_event_log_catalog[i].code == code) {
			return &g_event_log_catalog[i];
		}
	}
	return NULL;
}
