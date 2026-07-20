/*
 * event_log_ui.c — короткие русские строки для экрана журнала.
 */

#include "event_log_ui.h"

#include "backend.h"
#include "device_config.h"
#include "event_log_catalog.h"

#include <stdio.h>
#include <string.h>

extern PPKYCfg PPKYConfig;

static uint8_t BcdToBin(uint8_t bcd)
{
	return (uint8_t)(((bcd >> 4) & 0x0Fu) * 10u + (bcd & 0x0Fu));
}

static const char *McuTypeShort(uint8_t d_type)
{
	switch (d_type) {
	case DEVICE_MCU_TC_TYPE:  return "ТС";
	case DEVICE_MCU_IGN_TYPE: return "ИГН";
	case DEVICE_MCU_K1:       return "K1";
	case DEVICE_MCU_K2:       return "K2";
	case DEVICE_MCU_K3:       return "K3";
	case DEVICE_MCU_KR:       return "KR";
	case DEVICE_PPKY_TYPE:    return "ППКУ";
	case DEVICE_IGNITER_TYPE: return "СП";
	case DEVICE_DPT_TYPE:     return "ДПТ";
	case DEVICE_BUTTON_TYPE:  return "КН";
	case DEVICE_LSWITCH_TYPE: return "КЦ";
	case DEVICE_RELAY_TYPE:   return "РЛ";
	default:                  return "?";
	}
}

static void AppendZoneName(char *dst, size_t dst_sz, uint8_t zone)
{
	size_t len = strlen(dst);
	if (len + 1u >= dst_sz || zone == 0u) {
		return;
	}
	uint8_t zi = (uint8_t)(zone - 1u);
	if (zi >= ZONE_NUMBER) {
		(void)snprintf(dst + len, dst_sz - len, " з.%u", (unsigned)zone);
		return;
	}
	char zname[ZONE_NAME_SIZE + 1];
	size_t n = 0u;
	for (; n < ZONE_NAME_SIZE; n++) {
		char c = (char)PPKYConfig.zone_name[zi][n];
		if (c == '\0') {
			break;
		}
		zname[n] = c;
	}
	while (n > 0u && (zname[n - 1u] == ' ' || zname[n - 1u] == '\0')) {
		n--;
	}
	zname[n] = '\0';
	if (n == 0u) {
		(void)snprintf(dst + len, dst_sz - len, " з.%u", (unsigned)zone);
	} else {
		(void)snprintf(dst + len, dst_sz - len, " %s", zname);
	}
}

static void FormatCanDevice(char *dst, size_t dst_sz, uint32_t can_header)
{
	if (dst_sz == 0u) {
		return;
	}
	dst[0] = '\0';
	if (can_header == 0u) {
		return;
	}
	can_ext_id_t id;
	id.ID = can_header;
	uint8_t d_type = (uint8_t)(id.field.d_type & 0x7Fu);
	uint8_t h_adr = (uint8_t)id.field.h_adr;
	uint8_t l_adr = (uint8_t)(id.field.l_adr & 0x3Fu);
	uint8_t zone = (uint8_t)(id.field.zone & 0x7Fu);

	if (d_type == DEVICE_MCU_IGN_TYPE || d_type == DEVICE_MCU_TC_TYPE ||
	    d_type == DEVICE_MCU_K1 || d_type == DEVICE_MCU_K2 ||
	    d_type == DEVICE_MCU_K3 || d_type == DEVICE_MCU_KR) {
		(void)snprintf(dst, dst_sz, "МКУ %s:%u", McuTypeShort(d_type), (unsigned)h_adr);
	} else if (d_type == DEVICE_PPKY_TYPE) {
		(void)snprintf(dst, dst_sz, "ППКУ");
	} else {
		(void)snprintf(dst, dst_sz, "%s%u", McuTypeShort(d_type),
			       (unsigned)(l_adr != 0u ? l_adr : h_adr));
	}
	if (zone != 0u) {
		AppendZoneName(dst, dst_sz, zone);
	}
}

static void FormatHeaderPos(char *dst, size_t dst_sz, uint32_t index_1based, uint32_t count)
{
	if (count == 0u) {
		dst[0] = '\0';
		return;
	}
	if (count >= 10000u) {
		(void)snprintf(dst, dst_sz, "%lu/%luk",
			       (unsigned long)index_1based,
			       (unsigned long)((count + 999u) / 1000u));
	} else {
		(void)snprintf(dst, dst_sz, "%lu/%lu",
			       (unsigned long)index_1based,
			       (unsigned long)count);
	}
}

static const char *EventTitle(uint16_t code)
{
	switch (code) {
	case EVENT_LOG_MASTER_START:          return "СТАРТ М.";
	case EVENT_LOG_MASTER_STOP:           return "СТОП М.";
	case EVENT_LOG_SYSTEM_START_OK:       return "СТАРТ ОК";
	case EVENT_LOG_DEVICE_FAULT:          return "НЕИСПР.";
	case EVENT_LOG_FIRE_DETECTED:         return "ПОЖАР";
	case EVENT_LOG_EXTINGUISH_START:      return "ТУШЕНИЕ";
	case EVENT_LOG_EXTINGUISH_FORCE_STOP: return "ОСТ.ПУСК";
	case EVENT_LOG_EXTINGUISH_COMPLETE:   return "ТУШ.ОК";
	case EVENT_LOG_EXTINGUISH_INCOMPLETE: return "ТУШ.ОШ";
	case EVENT_LOG_PANEL_BUTTON:          return "КНОПКА";
	case EVENT_LOG_HOST_LINK:             return "СВЯЗЬ";
	case EVENT_LOG_CONFIG_APPLY_OK:       return "КОНФ.ОК";
	case EVENT_LOG_CONFIG_APPLY_FAIL:     return "КОНФ.ОШ";
	case EVENT_LOG_SOUND_TOGGLE:          return "ЗВУК";
	case EVENT_LOG_FIRE_MODE_CHANGE:      return "РЕЖИМ";
	case EVENT_LOG_FIRE_RESET:            return "СБРОС";
	default:                              return NULL;
	}
}

static void FormatDetail(const EventLogRecord_t *rec, char *dst, size_t dst_sz)
{
	dst[0] = '\0';
	const uint8_t *a = rec->additional;
	char dev[64];
	FormatCanDevice(dev, sizeof(dev), rec->can_header);

	switch (rec->event_code) {
	case EVENT_LOG_MASTER_START:
	case EVENT_LOG_MASTER_STOP:
	case EVENT_LOG_SYSTEM_START_OK:
		(void)snprintf(dst, dst_sz, "ППКУ");
		break;

	case EVENT_LOG_DEVICE_FAULT: {
		uint8_t fc = a[0];
		uint8_t ch = a[1];
		uint8_t phase = a[2];
		const char *ph = (phase != 0u) ? "-" : "+";
		if (fc == 6u) { /* position */
			(void)snprintf(dst, dst_sz, "%s ПОЗИЦИЯ МКУ %u", ph, (unsigned)ch);
		} else if (fc == 3u) { /* can */
			(void)snprintf(dst, dst_sz, "%s ОБРЫВ CAN%u", ph, (unsigned)(ch != 0u ? ch : 1u));
			if (dev[0] != '\0') {
				size_t n = strlen(dst);
				(void)snprintf(dst + n, dst_sz - n, " %s", dev);
			}
		} else if (fc == 4u) { /* power */
			const char *kind = (rec->can_data[0] != 0u) ? "ВВОД" : "ВЫХОД";
			(void)snprintf(dst, dst_sz, "%s %s %u", ph, kind, (unsigned)ch);
		} else if (fc == 1u) {
			(void)snprintf(dst, dst_sz, "%s КЗ", ph);
			if (dev[0] != '\0') {
				size_t n = strlen(dst);
				(void)snprintf(dst + n, dst_sz - n, " %s", dev);
			}
		} else if (fc == 0u) {
			(void)snprintf(dst, dst_sz, "%s ОБРЫВ", ph);
			if (dev[0] != '\0') {
				size_t n = strlen(dst);
				(void)snprintf(dst + n, dst_sz - n, " %s", dev);
			}
		} else {
			(void)snprintf(dst, dst_sz, "%s НЕИСП", ph);
			if (dev[0] != '\0') {
				size_t n = strlen(dst);
				(void)snprintf(dst + n, dst_sz - n, " %s", dev);
			}
		}
		break;
	}

	case EVENT_LOG_FIRE_DETECTED: {
		uint8_t zone = a[0];
		(void)snprintf(dst, dst_sz, "з.%u", (unsigned)zone);
		AppendZoneName(dst, dst_sz, zone);
		if (dev[0] != '\0') {
			size_t n = strlen(dst);
			(void)snprintf(dst + n, dst_sz - n, " %s", dev);
		}
		break;
	}

	case EVENT_LOG_EXTINGUISH_START: {
		const char *mode = "АВТО";
		if (a[0] == 0u) {
			mode = "РУЧН";
		} else if (a[0] == 2u) {
			mode = "АВТН";
		}
		if (a[1] != 0u) {
			(void)snprintf(dst, dst_sz, "%s з.%u", mode, (unsigned)a[1]);
			AppendZoneName(dst, dst_sz, a[1]);
		} else {
			(void)snprintf(dst, dst_sz, "%s все зоны", mode);
		}
		break;
	}

	case EVENT_LOG_EXTINGUISH_FORCE_STOP:
	case EVENT_LOG_EXTINGUISH_COMPLETE:
	case EVENT_LOG_EXTINGUISH_INCOMPLETE:
		if (a[1] != 0u || a[2] != 0u) {
			uint8_t zone = (rec->event_code == EVENT_LOG_EXTINGUISH_FORCE_STOP) ? a[1] :
				       (rec->event_code == EVENT_LOG_EXTINGUISH_COMPLETE) ? a[3] : a[2];
			if (zone != 0u) {
				(void)snprintf(dst, dst_sz, "з.%u", (unsigned)zone);
				AppendZoneName(dst, dst_sz, zone);
			} else {
				(void)snprintf(dst, dst_sz, "все зоны");
			}
		} else {
			(void)snprintf(dst, dst_sz, "все зоны");
		}
		break;

	case EVENT_LOG_PANEL_BUTTON: {
		const char *btn = "КНОПКА";
		if (a[0] == 0u) {
			btn = "ПУСК ОБЩИЙ";
		} else if (a[0] == 1u) {
			btn = "ПУСК СП";
		} else if (a[0] == 2u) {
			btn = "ОСТАНОВ";
		}
		if (a[1] != 0u) {
			(void)snprintf(dst, dst_sz, "%s з.%u", btn, (unsigned)a[1]);
		} else {
			(void)snprintf(dst, dst_sz, "%s", btn);
		}
		break;
	}

	case EVENT_LOG_HOST_LINK:
		(void)snprintf(dst, dst_sz, "%s", (a[0] == 1u) ? "RS-485" : "WiFi");
		break;

	case EVENT_LOG_CONFIG_APPLY_OK:
		(void)snprintf(dst, dst_sz, "OK %u/%u", (unsigned)a[0], (unsigned)a[1]);
		break;

	case EVENT_LOG_CONFIG_APPLY_FAIL: {
		const char *reason = "ошибка";
		if (a[0] == 0u) {
			reason = "таймаут";
		} else if (a[0] == 1u) {
			reason = "размер";
		} else if (a[0] == 2u) {
			reason = "эхо";
		} else if (a[0] == 3u) {
			reason = "CRC";
		}
		if (dev[0] != '\0') {
			(void)snprintf(dst, dst_sz, "%s %s", reason, dev);
		} else {
			(void)snprintf(dst, dst_sz, "%s", reason);
		}
		break;
	}

	case EVENT_LOG_SOUND_TOGGLE:
		(void)snprintf(dst, dst_sz, "%s", (a[0] != 0u) ? "ВКЛ" : "ВЫКЛ");
		break;

	case EVENT_LOG_FIRE_MODE_CHANGE: {
		const char *mode = "АВТО";
		if (a[0] == 1u) {
			mode = "АВТОНОМ";
		} else if (a[0] == 2u) {
			mode = "РУЧНОЙ";
		}
		(void)snprintf(dst, dst_sz, "%s", mode);
		break;
	}

	case EVENT_LOG_FIRE_RESET:
		if (a[0] != 0u) {
			(void)snprintf(dst, dst_sz, "з.%u", (unsigned)a[0]);
			AppendZoneName(dst, dst_sz, a[0]);
		} else {
			(void)snprintf(dst, dst_sz, "все зоны");
		}
		break;

	default:
		if (dev[0] != '\0') {
			(void)snprintf(dst, dst_sz, "%s", dev);
		}
		break;
	}
}

void EventLogUi_FormatEmpty(EventLogUiLines_t *out)
{
	if (out == NULL) {
		return;
	}
	(void)snprintf(out->header, sizeof(out->header), "ЖУРНАЛ");
	(void)snprintf(out->title, sizeof(out->title), "ПУСТО");
	(void)snprintf(out->detail, sizeof(out->detail), "Нет записей");
}

void EventLogUi_FormatRecord(const EventLogRecord_t *rec,
			     uint32_t display_index_1based,
			     uint32_t count,
			     EventLogUiLines_t *out)
{
	if (out == NULL) {
		return;
	}
	memset(out, 0, sizeof(*out));
	if (rec == NULL) {
		EventLogUi_FormatEmpty(out);
		return;
	}

	uint8_t yy = BcdToBin(rec->time[0]);
	uint8_t mo = BcdToBin(rec->time[1]);
	uint8_t dd = BcdToBin(rec->time[2]);
	uint8_t hh = BcdToBin(rec->time[3]);
	uint8_t mi = BcdToBin(rec->time[4]);

	char pos[16];
	FormatHeaderPos(pos, sizeof(pos), display_index_1based, count);
	/* Шапка TouchGFX = 32: «DD.MM.YY HH:MM n/N» */
	(void)snprintf(out->header, sizeof(out->header),
		       "%02u.%02u.%02u %02u:%02u %s",
		       (unsigned)dd, (unsigned)mo, (unsigned)yy,
		       (unsigned)hh, (unsigned)mi, pos);

	const char *title = EventTitle(rec->event_code);
	if (title != NULL) {
		(void)snprintf(out->title, sizeof(out->title), "%s", title);
	} else {
		(void)snprintf(out->title, sizeof(out->title), "СОБ.%u",
			       (unsigned)rec->event_code);
	}

	FormatDetail(rec, out->detail, sizeof(out->detail));
}
