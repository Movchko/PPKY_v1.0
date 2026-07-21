/*
 * event_log_ui.c — строки экрана «Журнал» (критический tier).
 *
 * Согласовано с warning.cpp BuildUiPayload() / главным экраном:
 *   центр  — big_titles (ОБРЫВ СП1, КЗ CAN2, ПОЖАР …)
 *   низ    — details (имя зоны, MKU K1, h_adr, S/N)
 *
 * Справочник title/detail — см. EventLogUi_FormatRecord() и таблицу ниже.
 */

#include "event_log_ui.h"

#include "backend.h"
#include "config_monitor.h"
#include "device_config.h"
#include "event_log_catalog.h"

#include <stdio.h>
#include <string.h>

extern PPKYCfg PPKYConfig;

/* fault_class в additional[0] для EVENT_LOG_DEVICE_FAULT (warning.cpp). */
enum {
	ELUI_FC_LINE_BREAK = 0u,
	ELUI_FC_LINE_SHORT = 1u,
	ELUI_FC_PROTOCOL   = 2u,
	ELUI_FC_CAN        = 3u,
	ELUI_FC_POWER      = 4u,
	ELUI_FC_OTHER      = 5u,
	ELUI_FC_POSITION   = 6u
};

static uint8_t BcdToBin(uint8_t bcd)
{
	return (uint8_t)(((bcd >> 4) & 0x0Fu) * 10u + (bcd & 0x0Fu));
}

static uint8_t IsMcuDType(uint8_t d_type)
{
	return (d_type == DEVICE_MCU_IGN_TYPE ||
	        d_type == DEVICE_MCU_TC_TYPE ||
	        d_type == DEVICE_MCU_K1 ||
	        d_type == DEVICE_MCU_K2 ||
	        d_type == DEVICE_MCU_K3 ||
	        d_type == DEVICE_MCU_KR) ? 1u : 0u;
}

static uint8_t IsVdevDType(uint8_t d_type)
{
	return (d_type == DEVICE_DPT_TYPE ||
	        d_type == DEVICE_IGNITER_TYPE ||
	        d_type == DEVICE_BUTTON_TYPE ||
	        d_type == DEVICE_LSWITCH_TYPE) ? 1u : 0u;
}

/* Как Warning_ChannelTypeShort(). */
static const char *ChannelTypeShort(uint8_t v_d_type)
{
	switch (v_d_type) {
	case DEVICE_DPT_TYPE:     return "ДПТ";
	case DEVICE_IGNITER_TYPE: return "СП";
	case DEVICE_BUTTON_TYPE:  return "КН";
	case DEVICE_LSWITCH_TYPE: return "КОН";
	default:                  return "???";
	}
}

/* Как McuTypeName() на главном экране. */
static const char *McuTypeName(uint8_t d_type)
{
	switch (d_type) {
	case DEVICE_MCU_IGN_TYPE: return "MKU IGN";
	case DEVICE_MCU_TC_TYPE:  return "MKU TC";
	case DEVICE_MCU_K1:       return "MKU K1";
	case DEVICE_MCU_K2:       return "MKU K2";
	case DEVICE_MCU_K3:       return "MKU K3";
	case DEVICE_MCU_KR:       return "MKU KR";
	default:                  return "MKU";
	}
}

static void GetZoneName(uint8_t zone, char *out, size_t out_sz)
{
	if (out_sz == 0u) {
		return;
	}
	out[0] = '\0';
	if (zone > 0u && zone <= ZONE_NUMBER) {
		strncpy(out, (const char *)PPKYConfig.zone_name[zone - 1u], ZONE_NAME_SIZE);
		out[ZONE_NAME_SIZE] = '\0';
	}
	if (out[0] == '\0') {
		snprintf(out, out_sz, "ЗОНА %u", (unsigned)zone);
	}
}

static void GetMkuSerial(uint8_t zone, uint8_t h_adr, uint8_t mcu_d_type,
			 char *out, size_t out_sz)
{
	for (uint8_t i = 0u; i < 32u; i++) {
		const Device *dv = &PPKYConfig.CfgDevices[i].UId.devId;
		if (dv->h_adr == h_adr && dv->d_type == mcu_d_type) {
			snprintf(out, out_sz, "S/N:%08lX:%08lX:%08lX",
				 (unsigned long)PPKYConfig.CfgDevices[i].UId.UId0,
				 (unsigned long)PPKYConfig.CfgDevices[i].UId.UId1,
				 (unsigned long)PPKYConfig.CfgDevices[i].UId.UId2);
			return;
		}
	}

	Device dev = {};
	dev.zone = zone;
	dev.h_adr = h_adr;
	dev.l_adr = 0u;
	dev.d_type = mcu_d_type;
	uint8_t remote_valid = 0u;
	const uint8_t *remote = ConfigMonitor_GetRemoteSerial(&dev, &remote_valid);
	if (remote_valid != 0u && remote != NULL) {
		const uint32_t *uid = (const uint32_t *)remote;
		snprintf(out, out_sz, "S/N:%08lX:%08lX:%08lX",
			 (unsigned long)uid[0], (unsigned long)uid[1], (unsigned long)uid[2]);
		return;
	}

	snprintf(out, out_sz, "S/N:---");
}

static void FormatPpkySerial(char *out, size_t out_sz)
{
	snprintf(out, out_sz, "S/N:%08lX:%08lX:%08lX",
		 (unsigned long)PPKYConfig.UId.UId0,
		 (unsigned long)PPKYConfig.UId.UId1,
		 (unsigned long)PPKYConfig.UId.UId2);
}

/* Как Warning_FormatMkuAndSerial(). */
static void ParseCanHeader(uint32_t can_header,
			   uint8_t *zone, uint8_t *h_adr, uint8_t *l_adr, uint8_t *d_type);

static void FormatMkuDetail(char *out, size_t out_sz,
			    uint8_t zone, uint8_t h_adr, uint8_t mcu_d_type)
{
	char serial[32];
	char zone_name[ZONE_NAME_SIZE + 1];
	GetMkuSerial(zone, h_adr, mcu_d_type, serial, sizeof(serial));
	GetZoneName(zone, zone_name, sizeof(zone_name));
	snprintf(out, out_sz, "%s %s %u %s",
		 zone_name, McuTypeName(mcu_d_type), (unsigned)h_adr, serial);
}

static void FormatSerialFromRecord(const EventLogRecord_t *rec, char *out, size_t out_sz)
{
	uint32_t uid0;
	uint32_t uid1;
	uint32_t uid2;

	memcpy(&uid0, rec->can_data, 4u);
	memcpy(&uid1, rec->can_data + 4u, 4u);
	memcpy(&uid2, rec->additional, 4u);
	snprintf(out, out_sz, "S/N:%08lX:%08lX:%08lX",
		 (unsigned long)uid0, (unsigned long)uid1, (unsigned long)uid2);
}

static void FormatMkuDetailFromRecord(const EventLogRecord_t *rec,
				      char *out, size_t out_sz)
{
	uint8_t zone = 0u;
	uint8_t h_adr = 0u;
	uint8_t l_adr = 0u;
	uint8_t d_type = 0u;
	char serial[32];
	char zone_name[ZONE_NAME_SIZE + 1];

	ParseCanHeader(rec->can_header, &zone, &h_adr, &l_adr, &d_type);
	FormatSerialFromRecord(rec, serial, sizeof(serial));
	GetZoneName(zone, zone_name, sizeof(zone_name));
	snprintf(out, out_sz, "%s %s %u %s",
		 zone_name, McuTypeName(d_type), (unsigned)h_adr, serial);
}

static void FormatPpkyDetail(char *out, size_t out_sz)
{
	char serial[32];
	FormatPpkySerial(serial, sizeof(serial));
	snprintf(out, out_sz, "ППКУ %s", serial);
}

static void FormatZoneOnlyDetail(char *out, size_t out_sz, uint8_t zone)
{
	if (zone == 0u) {
		snprintf(out, out_sz, "все зоны");
		return;
	}
	char zone_name[ZONE_NAME_SIZE + 1];
	GetZoneName(zone, zone_name, sizeof(zone_name));
	snprintf(out, out_sz, "%s", zone_name);
}

static uint8_t LookupMcuDType(uint8_t zone, uint8_t h_adr, uint8_t fallback)
{
	for (uint8_t i = 0u; i < 32u; i++) {
		const Device *dv = &PPKYConfig.CfgDevices[i].UId.devId;
		if (!IsMcuDType(dv->d_type)) {
			continue;
		}
		if (dv->h_adr == h_adr && (zone == 0u || dv->zone == zone)) {
			return dv->d_type;
		}
	}
	return fallback;
}

static void ParseCanHeader(uint32_t can_header,
			   uint8_t *zone, uint8_t *h_adr, uint8_t *l_adr, uint8_t *d_type)
{
	can_ext_id_t id;
	id.ID = can_header;
	*zone = (uint8_t)(id.field.zone & 0x7Fu);
	*h_adr = (uint8_t)id.field.h_adr;
	*l_adr = (uint8_t)(id.field.l_adr & 0x3Fu);
	*d_type = (uint8_t)(id.field.d_type & 0x7Fu);
}

static void FormatHeaderPos(char *dst, size_t dst_sz, uint32_t index_1based, uint32_t count)
{
	if (count == 0u) {
		dst[0] = '\0';
		return;
	}
	if (count >= 10000u) {
		snprintf(dst, dst_sz, "%lu/%luk",
			 (unsigned long)index_1based,
			 (unsigned long)((count + 999u) / 1000u));
	} else {
		snprintf(dst, dst_sz, "%lu/%lu",
			 (unsigned long)index_1based,
			 (unsigned long)count);
	}
}

/*
 * EVENT_LOG_DEVICE_FAULT (8) — title как BuildUiPayload:
 *   vdev:     ОБРЫВ|КЗ|НЕИСП + ChannelTypeShort + l_adr
 *   MCU CAN:  ОБРЫВ|КЗ + CANn
 *   PPKU CAN: ОБРЫВ CANn
 *   power:    ВЫХОД n | ПИТАНИЕ n
 *   position: ПОЗИЦИЯ
 */
static void FormatDeviceFaultTitle(const EventLogRecord_t *rec, char *title, size_t title_sz)
{
	const uint8_t *a = rec->additional;
	uint8_t fc = a[0];
	uint8_t ch = a[1];
	uint8_t zone = 0u;
	uint8_t h_adr = 0u;
	uint8_t l_adr = 0u;
	uint8_t d_type = 0u;

	ParseCanHeader(rec->can_header, &zone, &h_adr, &l_adr, &d_type);

	if (fc == ELUI_FC_POSITION) {
		snprintf(title, title_sz, "ПОЗИЦИЯ");
		return;
	}

	if (fc == ELUI_FC_POWER) {
		if (rec->can_data[0] != 0u) {
			snprintf(title, title_sz, "ПИТАНИЕ %u", (unsigned)(ch != 0u ? ch : 1u));
		} else {
			snprintf(title, title_sz, "ВЫХОД %u", (unsigned)(ch != 0u ? ch : 1u));
		}
		return;
	}

	if (fc == ELUI_FC_CAN) {
		uint8_t line_state = rec->can_data[0];
		const char *fault = (line_state == 2u) ? "КЗ" : "ОБРЫВ";
		uint8_t can_idx = (ch != 0u) ? ch : rec->can_data[1];
		if (can_idx == 0u) {
			can_idx = 1u;
		}
		if (IsMcuDType(d_type)) {
			snprintf(title, title_sz, "%s CAN%u", fault, (unsigned)can_idx);
		} else {
			snprintf(title, title_sz, "%s CAN%u", fault, (unsigned)can_idx);
		}
		return;
	}

	if (IsVdevDType(d_type)) {
		const char *fault = "ОБРЫВ";
		if (fc == ELUI_FC_LINE_SHORT) {
			fault = "КЗ";
		} else if (fc == ELUI_FC_PROTOCOL || fc == ELUI_FC_OTHER) {
			fault = "НЕИСП";
		}
		uint8_t v_l = (l_adr != 0u) ? l_adr : ch;
		snprintf(title, title_sz, "%s %s%u", fault, ChannelTypeShort(d_type), (unsigned)v_l);
		return;
	}

	snprintf(title, title_sz, "НЕИСПР.");
}

static void FormatDeviceFaultDetail(const EventLogRecord_t *rec, char *detail, size_t detail_sz)
{
	const uint8_t *a = rec->additional;
	uint8_t fc = a[0];
	uint8_t ch = a[1];
	uint8_t zone = 0u;
	uint8_t h_adr = 0u;
	uint8_t l_adr = 0u;
	uint8_t d_type = 0u;

	ParseCanHeader(rec->can_header, &zone, &h_adr, &l_adr, &d_type);

	if (fc == ELUI_FC_POSITION) {
		snprintf(detail, detail_sz, "МКУ %u", (unsigned)(ch != 0u ? ch : rec->can_data[0]));
		return;
	}

	if (fc == ELUI_FC_POWER || (fc == ELUI_FC_CAN && d_type == DEVICE_PPKY_TYPE)) {
		FormatPpkyDetail(detail, detail_sz);
		return;
	}

	if (IsMcuDType(d_type)) {
		FormatMkuDetail(detail, detail_sz, zone, h_adr, d_type);
		return;
	}

	if (IsVdevDType(d_type)) {
		uint8_t mcu_d_type = LookupMcuDType(zone, h_adr, DEVICE_MCU_K1);
		FormatMkuDetail(detail, detail_sz, zone, h_adr, mcu_d_type);
		return;
	}

	if (rec->can_header != 0u) {
		uint8_t mcu_d_type = LookupMcuDType(zone, h_adr, d_type);
		if (IsMcuDType(mcu_d_type)) {
			FormatMkuDetail(detail, detail_sz, zone, h_adr, mcu_d_type);
			return;
		}
	}

	FormatPpkyDetail(detail, detail_sz);
}

static void FormatTitle(const EventLogRecord_t *rec, char *title, size_t title_sz)
{
	const uint8_t *a = rec->additional;

	switch (rec->event_code) {
	case EVENT_LOG_DEVICE_FAULT:
		FormatDeviceFaultTitle(rec, title, title_sz);
		break;

	case EVENT_LOG_FIRE_DETECTED:
		snprintf(title, title_sz, "ПОЖАР");
		break;

	case EVENT_LOG_EXTINGUISH_START:
		snprintf(title, title_sz, "ТУШЕНИЕ");
		break;

	case EVENT_LOG_EXTINGUISH_FORCE_STOP:
		snprintf(title, title_sz, "ПОЖАР/ОСТ.");
		break;

	case EVENT_LOG_EXTINGUISH_COMPLETE:
		snprintf(title, title_sz, "ТУШ.ВЫП.");
		break;

	case EVENT_LOG_EXTINGUISH_INCOMPLETE:
		snprintf(title, title_sz, "ТУШ.ОШ.");
		break;

	case EVENT_LOG_PANEL_BUTTON:
		if (a[0] == 0u) {
			snprintf(title, title_sz, "ПУСК ОБЩИЙ");
		} else if (a[0] == 1u) {
			snprintf(title, title_sz, "ПУСК СП");
		} else if (a[0] == 2u) {
			snprintf(title, title_sz, "ОСТАНОВ");
		} else {
			snprintf(title, title_sz, "КНОПКА");
		}
		break;

	case EVENT_LOG_MASTER_START:
		snprintf(title, title_sz, "СТАРТ");
		break;

	case EVENT_LOG_MASTER_STOP:
		snprintf(title, title_sz, "СТОП");
		break;

	case EVENT_LOG_SYSTEM_START_OK:
		snprintf(title, title_sz, "СТАРТ ОК");
		break;

	case EVENT_LOG_HOST_LINK:
		snprintf(title, title_sz, "СВЯЗЬ");
		break;

	case EVENT_LOG_CONFIG_APPLY_OK:
		snprintf(title, title_sz, "КОНФ.ОК");
		break;

	case EVENT_LOG_CONFIG_APPLY_FAIL:
		snprintf(title, title_sz, "КОНФ.ОШ");
		break;

	case EVENT_LOG_SOUND_TOGGLE:
		snprintf(title, title_sz, "ЗВУК");
		break;

	case EVENT_LOG_FIRE_MODE_CHANGE:
		snprintf(title, title_sz, "РЕЖИМ");
		break;

	case EVENT_LOG_FIRE_RESET:
		snprintf(title, title_sz, "СБРОС");
		break;

	case EVENT_LOG_MCU_SAVED:
		snprintf(title, title_sz, "СОХР.МКУ");
		break;

	default:
		snprintf(title, title_sz, "СОБ.%u", (unsigned)rec->event_code);
		break;
	}
}

/*
 * Фаза неисправности (DEVICE_FAULT additional[2]):
 *   0 — появилось  → «-ОБРЫВ СП1»
 *   1 — устранено  → «+ОБРЫВ СП1»
 */
static void PrependPhasePrefix(const EventLogRecord_t *rec, char *title, size_t title_sz)
{
	if (title_sz == 0u || title[0] == '\0') {
		return;
	}
	if (rec->event_code != EVENT_LOG_DEVICE_FAULT) {
		return;
	}
	char prefix = (rec->additional[2] != 0u) ? '+' : '-';
	char tmp[EVENT_LOG_UI_TITLE_LEN];
	snprintf(tmp, sizeof(tmp), "%c%s", prefix, title);
	strncpy(title, tmp, title_sz);
	title[title_sz - 1u] = '\0';
}

static void FormatDetail(const EventLogRecord_t *rec, char *detail, size_t detail_sz)
{
	const uint8_t *a = rec->additional;
	uint8_t zone = 0u;
	uint8_t h_adr = 0u;
	uint8_t l_adr = 0u;
	uint8_t d_type = 0u;

	detail[0] = '\0';
	ParseCanHeader(rec->can_header, &zone, &h_adr, &l_adr, &d_type);

	switch (rec->event_code) {
	case EVENT_LOG_DEVICE_FAULT:
		FormatDeviceFaultDetail(rec, detail, detail_sz);
		break;

	case EVENT_LOG_FIRE_DETECTED:
		FormatZoneOnlyDetail(detail, detail_sz, a[0]);
		if (rec->can_header != 0u && IsMcuDType(d_type)) {
			char mku[96];
			FormatMkuDetail(mku, sizeof(mku), zone, h_adr, d_type);
			snprintf(detail, detail_sz, "%s", mku);
		} else if (rec->can_header != 0u && IsVdevDType(d_type)) {
			uint8_t mcu_d_type = LookupMcuDType(zone, h_adr, DEVICE_MCU_K1);
			FormatMkuDetail(detail, detail_sz, zone, h_adr, mcu_d_type);
		}
		break;

	case EVENT_LOG_EXTINGUISH_START:
	case EVENT_LOG_EXTINGUISH_FORCE_STOP:
	case EVENT_LOG_EXTINGUISH_COMPLETE:
	case EVENT_LOG_EXTINGUISH_INCOMPLETE:
	case EVENT_LOG_FIRE_RESET: {
		uint8_t z = 0u;
		if (rec->event_code == EVENT_LOG_EXTINGUISH_START) {
			z = a[1];
		} else if (rec->event_code == EVENT_LOG_EXTINGUISH_FORCE_STOP) {
			z = a[1];
		} else if (rec->event_code == EVENT_LOG_EXTINGUISH_COMPLETE) {
			z = a[3];
		} else if (rec->event_code == EVENT_LOG_EXTINGUISH_INCOMPLETE) {
			z = a[2];
		} else {
			z = a[0];
		}
		FormatZoneOnlyDetail(detail, detail_sz, z);
		break;
	}

	case EVENT_LOG_PANEL_BUTTON:
		if (a[1] != 0u) {
			FormatZoneOnlyDetail(detail, detail_sz, a[1]);
		} else {
			FormatPpkyDetail(detail, detail_sz);
		}
		break;

	case EVENT_LOG_MASTER_START:
	case EVENT_LOG_MASTER_STOP:
	case EVENT_LOG_SYSTEM_START_OK:
	case EVENT_LOG_HOST_LINK:
	case EVENT_LOG_SOUND_TOGGLE:
	case EVENT_LOG_FIRE_MODE_CHANGE:
		FormatPpkyDetail(detail, detail_sz);
		break;

	case EVENT_LOG_CONFIG_APPLY_OK:
		snprintf(detail, detail_sz, "ППКУ OK %u/%u", (unsigned)a[0], (unsigned)a[1]);
		break;

	case EVENT_LOG_CONFIG_APPLY_FAIL:
		if (IsMcuDType(d_type)) {
			FormatMkuDetail(detail, detail_sz, zone, h_adr, d_type);
		} else {
			FormatPpkyDetail(detail, detail_sz);
		}
		break;

	case EVENT_LOG_MCU_SAVED:
		if (IsMcuDType(d_type)) {
			FormatMkuDetailFromRecord(rec, detail, detail_sz);
		}
		break;

	default:
		if (IsMcuDType(d_type)) {
			FormatMkuDetail(detail, detail_sz, zone, h_adr, d_type);
		} else if (rec->can_header != 0u) {
			uint8_t mcu_d_type = LookupMcuDType(zone, h_adr, d_type);
			if (IsMcuDType(mcu_d_type)) {
				FormatMkuDetail(detail, detail_sz, zone, h_adr, mcu_d_type);
			} else {
				FormatPpkyDetail(detail, detail_sz);
			}
		} else {
			FormatPpkyDetail(detail, detail_sz);
		}
		break;
	}
}

void EventLogUi_FormatEmpty(EventLogUiLines_t *out)
{
	if (out == NULL) {
		return;
	}
	snprintf(out->header, sizeof(out->header), "ЖУРНАЛ");
	snprintf(out->title, sizeof(out->title), "ПУСТО");
	snprintf(out->detail, sizeof(out->detail), "Нет записей");
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
	snprintf(out->header, sizeof(out->header),
		 "%s %02u.%02u.%02u %02u:%02u",
		 pos, (unsigned)dd, (unsigned)mo, (unsigned)yy,
		 (unsigned)hh, (unsigned)mi);

	FormatTitle(rec, out->title, sizeof(out->title));
	PrependPhasePrefix(rec, out->title, sizeof(out->title));
	FormatDetail(rec, out->detail, sizeof(out->detail));
}
