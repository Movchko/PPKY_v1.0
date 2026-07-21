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
#include "backend.h"

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

/**
 * @brief Событие 7 TELEMETRY: уникальный CAN RX после дедупа кольца.
 *        can_header + can_data; additional[0]=0 (raw_status).
 */
void EventLog_LogCanTelemetry(uint32_t can_id, const uint8_t *data);

/** media: 0=WiFi(UART2), 1=RS485(UART4). Одна запись на сессию канала. */
void EventLog_LogHostLink(uint8_t media);

/** Сброс сессии HOST_LINK (вызывать при Esp32_SetEnabled(0) для WiFi). */
void EventLog_HostLinkSessionReset(uint8_t media);

/** Успешная заливка конфига во все целевые МКУ (APPLY). */
void EventLog_LogConfigApplyOk(uint8_t mcu_ok_count, uint8_t mcu_total);

/**
 * Сохранено МКУ: can_header = ID МКУ (dir=1), can_data/additional = серийник (UId0..UId2).
 * Критический tier.
 */
void EventLog_LogMcuSaved(const Device *dev, const UniqId *uid);

/** По одному MCU_SAVED на каждый слот PPKYConfig.CfgDevices[] с типом МКУ. */
void EventLog_LogAllCfgMcusSaved(void);

/**
 * Не удалось залить конфиг в конкретный МКУ.
 * reason: 0=timeout, 1=bad_size, 2=echo_mismatch, 3=crc_mismatch.
 */
void EventLog_LogConfigApplyFail(uint8_t d_type, uint8_t h_adr, uint8_t l_adr, uint8_t zone,
                                 uint8_t slot, uint8_t reason);

/** Меню: звук вкл/выкл. enabled: 0=откл, 1=вкл. source: 0=menu. */
void EventLog_LogSoundToggle(uint8_t enabled, uint8_t source);

/** Меню: смена режима. mode: 0=auto, 1=autonomous, 2=manual. source: 0=menu. */
void EventLog_LogFireModeChange(uint8_t mode, uint8_t source);

/**
 * Период выборочной телеметрии (мс). Менять здесь — не в теле тика.
 * По умолчанию 10 минут.
 */
#ifndef EVENT_LOG_TELEMETRY_SAMPLE_PERIOD_MS
#define EVENT_LOG_TELEMETRY_SAMPLE_PERIOD_MS  (10u * 60u * 1000u)
#endif

/** Бюджет записей Flash на один вызов 1мс-тика во время снимка. */
#ifndef EVENT_LOG_TELEMETRY_SAMPLE_BUDGET
#define EVENT_LOG_TELEMETRY_SAMPLE_BUDGET     4u
#endif

/**
 * Периодический снимок статусов МКУ + вирт. устройств (код 20).
 * Вызывать из AppTimer1ms.
 */
void EventLog_ProcessTelemetrySample(uint32_t now_ms);

EventLogTier_t *EventLog_GetCriticalTier(void);
EventLogTier_t *EventLog_GetGeneralTier(void);

#ifdef __cplusplus
}
#endif

#endif /* INC_EVENT_LOG_H_ */
