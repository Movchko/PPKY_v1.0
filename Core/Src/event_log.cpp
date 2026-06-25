/*
 * event_log.cpp
 *
 * Двухуровневый логер событий ППКУ: критический и общий.
 */

#include "event_log.h"
#include "app.hpp"
#include "rtc_cache.h"

#include <string.h>

static EventLogTier_t g_critical_tier;
static EventLogTier_t g_general_tier;
static EventLogCapacityInfo_t g_capacity;
static uint8_t g_debug_enabled = 0u;
static bool g_initialized = false;

static_assert(FLASH_LOG_GENERAL_END_SECTOR < SPI_FLASH_SECTOR_COUNT,
              "Event log region exceeds SPI flash size");
static_assert(FLASH_LOG_CRITICAL_SECTORS > 0u && FLASH_LOG_GENERAL_SECTORS > 0u,
              "No SPI flash sectors left for event log after config region");
static_assert(FLASH_LOG_CRITICAL_RECORDS >= FLASH_LOG_CRITICAL_MIN_RECORDS,
              "Critical event log capacity below minimum");
static_assert(FLASH_LOG_GENERAL_END_SECTOR == (FLASH_LOG_START_SECTOR + FLASH_LOG_TOTAL_SECTORS - 1u),
              "Event log regions must cover all log sectors without gaps");

static void EventLog_FillCapacityInfo(void)
{
	g_capacity.log_start_sector = FLASH_LOG_START_SECTOR;
	g_capacity.log_total_sectors = FLASH_LOG_TOTAL_SECTORS;
	g_capacity.critical_sectors = FLASH_LOG_CRITICAL_SECTORS;
	g_capacity.general_sectors = FLASH_LOG_GENERAL_SECTORS;
	g_capacity.unused_tail_sectors = FLASH_LOG_UNUSED_TAIL_SECTORS;

	g_capacity.critical_start_sector = FLASH_LOG_CRITICAL_START_SECTOR;
	g_capacity.critical_end_sector = FLASH_LOG_CRITICAL_END_SECTOR;
	g_capacity.general_start_sector = FLASH_LOG_GENERAL_START_SECTOR;
	g_capacity.general_end_sector = FLASH_LOG_GENERAL_END_SECTOR;

	g_capacity.critical_record_capacity = FLASH_LOG_CRITICAL_RECORDS;
	g_capacity.general_record_capacity = FLASH_LOG_GENERAL_RECORDS;
	g_capacity.records_lost_on_sector_wrap = EVENT_LOG_RECORDS_PER_SECTOR;
}

static void EventLog_FillTimeBcd(uint8_t time[6])
{
	RTC_TimeTypeDef time_bcd;
	RTC_DateTypeDef date_bcd;

	if (time == NULL) {
		return;
	}

	if (!RtcCache_GetBcd(&time_bcd, &date_bcd)) {
		memset(time, 0, 6);
		return;
	}

	time[0] = date_bcd.Year;
	time[1] = date_bcd.Month;
	time[2] = date_bcd.Date;
	time[3] = time_bcd.Hours;
	time[4] = time_bcd.Minutes;
	time[5] = time_bcd.Seconds;
}

static bool EventLog_WriteToTier(EventLogTier_t *tier,
                                 uint16_t code,
                                 const EventLogPayload_t *payload)
{
	EventLogRecord_t record;

	memset(&record, 0, sizeof(record));
	EventLog_FillTimeBcd(record.time);
	record.event_code = code;
	record.reserved = 0u;

	if (payload != NULL) {
		record.master_wagon_num = payload->master_wagon_num;
		record.can_header = payload->can_header;
		memcpy(record.can_data, payload->can_data, sizeof(record.can_data));
		memcpy(record.additional, payload->additional, sizeof(record.additional));
	}

	return EventLogTier_Write(tier, &record);
}

bool EventLog_Init(SPIF_HandleTypeDef *spif_handle)
{
	bool ok_critical;
	bool ok_general;

	EventLog_FillCapacityInfo();

	ok_critical = EventLogTier_Init(&g_critical_tier,
	                                spif_handle,
	                                FLASH_LOG_CRITICAL_START_SECTOR,
	                                FLASH_LOG_CRITICAL_END_SECTOR);
	ok_general = EventLogTier_Init(&g_general_tier,
	                               spif_handle,
	                               FLASH_LOG_GENERAL_START_SECTOR,
	                               FLASH_LOG_GENERAL_END_SECTOR);

	g_initialized = ok_critical && ok_general;
	return g_initialized;
}

bool EventLog_IsInitialized(void)
{
	return g_initialized;
}

void EventLog_SetDebug(uint8_t enabled)
{
	g_debug_enabled = (enabled != 0u) ? 1u : 0u;
}

uint8_t EventLog_GetDebug(void)
{
	return g_debug_enabled;
}

const EventLogCapacityInfo_t *EventLog_GetCapacityInfo(void)
{
	return &g_capacity;
}

bool EventLog_Post(uint16_t code, const EventLogPayload_t *payload)
{
	const EventLogDescriptor_t *desc;
	EventLogTier_t *tier;

	if (!g_initialized) {
		return false;
	}

	desc = EventLogCatalog_Find(code);
	if (desc == NULL) {
		return false;
	}

	if (desc->debug_only != 0u && g_debug_enabled == 0u) {
		return true;
	}

	tier = (desc->level == EVENT_LOG_LEVEL_CRITICAL) ? &g_critical_tier : &g_general_tier;
	return EventLog_WriteToTier(tier, code, payload);
}

EventLogTier_t *EventLog_GetCriticalTier(void)
{
	return &g_critical_tier;
}

EventLogTier_t *EventLog_GetGeneralTier(void)
{
	return &g_general_tier;
}
