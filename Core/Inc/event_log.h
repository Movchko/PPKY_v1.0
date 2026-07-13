/*
 * event_log.h
 *
 * Высокоуровневый API логера событий ППКУ (два уровня: критический и общий).
 * Точки вызова в прикладном коде подключаются позже через EventLog_Post().
 */

#ifndef INC_EVENT_LOG_H_
#define INC_EVENT_LOG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include "event_logger.h"
#include "event_log_catalog.h"
#include "spif.h"

typedef struct {
	uint32_t log_start_sector;
	uint32_t log_total_sectors;

	uint32_t critical_sectors;
	uint32_t general_sectors;
	uint32_t unused_tail_sectors;

	uint32_t critical_start_sector;
	uint32_t critical_end_sector;
	uint32_t general_start_sector;
	uint32_t general_end_sector;

	uint32_t critical_record_capacity;
	uint32_t general_record_capacity;
	uint32_t records_lost_on_sector_wrap;
} EventLogCapacityInfo_t;

typedef struct {
	uint8_t  master_wagon_num;
	uint32_t can_header;
	uint8_t  can_data[8];
	uint8_t  additional[8];
} EventLogPayload_t;

bool EventLog_Init(SPIF_HandleTypeDef *spif_handle);
bool EventLog_IsInitialized(void);

void EventLog_SetDebug(uint8_t enabled);
uint8_t EventLog_GetDebug(void);

const EventLogCapacityInfo_t *EventLog_GetCapacityInfo(void);

/**
 * @brief Запись события по коду из каталога.
 * @param code Код события (EventLogCode_t).
 * @param payload Поля записи (время и CRC заполняются автоматически).
 * @return true при успешной записи или если событие отфильтровано (debug-only);
 *         false при ошибке Flash или неизвестном коде.
 */
bool EventLog_Post(uint16_t code, const EventLogPayload_t *payload);

/**
 * @brief Запись события с явной меткой времени (BCD, как в record.time).
 * @param time_bcd 6 байт: YY MM DD HH MM SS (формат RTC).
 */
bool EventLog_PostAt(const uint8_t time_bcd[6],
                     uint16_t code,
                     const EventLogPayload_t *payload);

/**
 * @brief События 2 и 1 при включении: выключение (время из RTC_BKP_DR1),
 *        затем запуск (текущее RTC). Вызывать один раз после EventLog_Init().
 */
void EventLog_LogMasterBoot(void);

EventLogTier_t *EventLog_GetCriticalTier(void);
EventLogTier_t *EventLog_GetGeneralTier(void);

#ifdef __cplusplus
}
#endif

#endif /* INC_EVENT_LOG_H_ */
