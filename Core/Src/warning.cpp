#include "warning.h"

#include <cstdio>
#include <cstring>

#include "app.hpp"
#include "fire.h"
#include "led.h"
#include "can_bus.h"
#include "device_config.h"

extern ActiveDeviceInfo g_active_devices[NUM_ACTIVE_DEVICE];
extern uint8_t g_active_devices_count;
extern PPKYCfg PPKYConfig;

extern "C" void Warning_UiUpdate(uint8_t active, uint8_t n_items,
				 char (*big_titles)[16],
				 char (*details)[ZONE_NAME_SIZE + 1]);

namespace {

constexpr uint8_t WARN_MAX_ITEMS = 16u;
constexpr uint32_t WARNING_SHOW_HOLD_MS = 10000u; /* Показывать предупреждение ещё 10 с после исчезновения */
constexpr uint32_t LED_ERR_OFF_DELAY_MS = 5000u;  /* Гасить LED_ERR через 5 с после исчезновения всех неисправностей */

struct WarningItem {
	uint8_t used;
	uint8_t kind; /* 0=vdev line fault, 1=mcu can fault, 2=ppku can fault */
	uint8_t zone;
	uint8_t h_adr;
	uint8_t v_l_adr;
	uint8_t mcu_d_type;
	uint8_t v_d_type;
	uint8_t line_state; /* 1=обрыв, 2=КЗ */
	uint8_t can_idx; /* 1 или 2 для CAN-предупреждений */
	uint8_t fault_now;
	uint32_t show_until_ms;
};

static WarningItem g_items[WARN_MAX_ITEMS];

static uint8_t g_last_active = 0xFFu;
static uint8_t g_last_count = 0xFFu;
static char g_last_big[WARN_MAX_ITEMS][16];
static char g_last_details[WARN_MAX_ITEMS][ZONE_NAME_SIZE + 1];
static uint8_t g_led_err_on = 0u;
static uint32_t g_led_err_off_deadline_ms = 0u;

/* Текстовое имя типа МКУ для отображения в UI предупреждений. */
static const char* McuTypeName(uint8_t d_type)
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

/* Фильтр типов виртуальных устройств, участвующих в модуле неисправностей. */
static uint8_t IsTrackedVdevType(uint8_t v_d_type)
{
	return (v_d_type == DEVICE_DPT_TYPE || v_d_type == DEVICE_IGNITER_TYPE) ? 1u : 0u;
}

/* Проверяет, что состояние линии относится к неисправности (обрыв/КЗ). */
static uint8_t IsFaultLineState(uint8_t line_state)
{
	return (line_state == 1u || line_state == 2u) ? 1u : 0u; /* 1=Обрыв, 2=КЗ */
}

/* Поиск записи неисправности по ключу устройства/канала. */
static int FindItem(uint8_t kind, uint8_t zone, uint8_t h_adr, uint8_t v_l_adr,
		    uint8_t mcu_d_type, uint8_t v_d_type, uint8_t can_idx)
{
	for (uint8_t i = 0u; i < WARN_MAX_ITEMS; i++) {
		if (!g_items[i].used) {
			continue;
		}
		if (g_items[i].kind == kind &&
		    g_items[i].zone == zone &&
		    g_items[i].h_adr == h_adr &&
		    g_items[i].v_l_adr == v_l_adr &&
		    g_items[i].mcu_d_type == mcu_d_type &&
		    g_items[i].v_d_type == v_d_type &&
		    g_items[i].can_idx == can_idx) {
			return (int)i;
		}
	}
	return -1;
}

/* Удаляет запись неисправности по индексу. */
static void RemoveItemAt(uint8_t idx)
{
	if (idx < WARN_MAX_ITEMS) {
		memset(&g_items[idx], 0, sizeof(g_items[idx]));
	}
}

/* Безопасное сравнение таймера с учётом переполнения HAL_GetTick(). */
static uint8_t TimeReached(uint32_t now_ms, uint32_t deadline_ms)
{
	return ((int32_t)(now_ms - deadline_ms) >= 0) ? 1u : 0u;
}

/* Добавляет/обновляет запись неисправности и продлевает окно отображения. */
static void UpsertItem(uint8_t kind, uint8_t zone, uint8_t h_adr, uint8_t v_l_adr,
		       uint8_t mcu_d_type, uint8_t v_d_type, uint8_t line_state,
		       uint8_t can_idx, uint32_t now_ms)
{
	int idx = FindItem(kind, zone, h_adr, v_l_adr, mcu_d_type, v_d_type, can_idx);
	if (idx >= 0) {
		g_items[(uint8_t)idx].line_state = line_state;
		g_items[(uint8_t)idx].can_idx = can_idx;
		g_items[(uint8_t)idx].fault_now = 1u;
		g_items[(uint8_t)idx].show_until_ms = now_ms + WARNING_SHOW_HOLD_MS;
		return;
	}
	for (uint8_t i = 0u; i < WARN_MAX_ITEMS; i++) {
		if (g_items[i].used) {
			continue;
		}
		g_items[i].used = 1u;
		g_items[i].kind = kind;
		g_items[i].zone = zone;
		g_items[i].h_adr = h_adr;
		g_items[i].v_l_adr = v_l_adr;
		g_items[i].mcu_d_type = mcu_d_type;
		g_items[i].v_d_type = v_d_type;
		g_items[i].line_state = line_state;
		g_items[i].can_idx = can_idx;
		g_items[i].fault_now = 1u;
		g_items[i].show_until_ms = now_ms + WARNING_SHOW_HOLD_MS;
		return;
	}
}

/* Помечает неисправность как восстановленную (с хвостом отображения). */
static void MarkRecovered(uint8_t kind, uint8_t zone, uint8_t h_adr, uint8_t v_l_adr,
			  uint8_t mcu_d_type, uint8_t v_d_type, uint8_t can_idx, uint32_t now_ms)
{
	int idx = FindItem(kind, zone, h_adr, v_l_adr, mcu_d_type, v_d_type, can_idx);
	if (idx >= 0) {
		g_items[(uint8_t)idx].fault_now = 0u;
		g_items[(uint8_t)idx].show_until_ms = now_ms + WARNING_SHOW_HOLD_MS;
	}
}

/* Проверяет, что запись всё ещё реально неисправна по текущим данным шины. */
static uint8_t IsItemStillFaulty(const WarningItem& it)
{
	for (uint8_t mi = 0u; mi < g_active_devices_count; mi++) {
		ActiveDeviceInfo* m = &g_active_devices[mi];
		if (!m->online) {
			continue;
		}
		if (m->dev.zone != it.zone || m->dev.h_adr != it.h_adr || m->dev.d_type != it.mcu_d_type) {
			continue;
		}
		for (uint8_t vi = 0u; vi < m->vdev_count; vi++) {
			auto* v = &m->vdevs[vi];
			if (!v->online) {
				continue;
			}
			if (it.kind == 0u && v->v_l_adr == it.v_l_adr && v->v_d_type == it.v_d_type) {
				return (IsTrackedVdevType(v->v_d_type) && IsFaultLineState(v->line_state)) ? 1u : 0u;
			}
		}
	}
	if (it.kind == 1u) {
		for (uint8_t mi = 0u; mi < g_active_devices_count; mi++) {
			ActiveDeviceInfo* m = &g_active_devices[mi];
			if (!m->online) {
				continue;
			}
			if (m->dev.zone == it.zone && m->dev.h_adr == it.h_adr && m->dev.d_type == it.mcu_d_type) {
				if (!m->can_status_valid) {
					return 0u;
				}
				return ((m->can_status_mask & (1u << (it.can_idx - 1u))) == 0u) ? 1u : 0u;
			}
		}
	}
	if (it.kind == 2u) {
		return ((can_bus_error_flags & (1u << (it.can_idx - 1u))) != 0u) ? 1u : 0u;
	}
	return 0u;
}

/* Обрабатывает только vdev со status_changed и обновляет список предупреждений. */
static void ConsumeChangedStatuses(uint32_t now_ms)
{
	for (uint8_t mi = 0u; mi < g_active_devices_count; mi++) {
		ActiveDeviceInfo* m = &g_active_devices[mi];
		if (!m->online) {
			continue;
		}
		for (uint8_t vi = 0u; vi < m->vdev_count; vi++) {
			auto* v = &m->vdevs[vi];
			if (!v->online) {
				continue;
			}
			if (!v->status_changed) {
				continue;
			}
			if (!IsTrackedVdevType(v->v_d_type)) {
				v->status_changed = 0u;
				continue;
			}

			if (IsFaultLineState(v->line_state)) {
				UpsertItem(0u, m->dev.zone, m->dev.h_adr, v->v_l_adr, m->dev.d_type, v->v_d_type, v->line_state, 0u, now_ms);
			} else {
				MarkRecovered(0u, m->dev.zone, m->dev.h_adr, v->v_l_adr, m->dev.d_type, v->v_d_type, 0u, now_ms);
			}
			v->status_changed = 0u;
		}
	}
}

/* Снимает/удаляет устаревшие записи, управляет 10-секундным хвостом показа. */
static void PruneInactiveItems(uint32_t now_ms)
{
	for (uint8_t i = 0u; i < WARN_MAX_ITEMS; i++) {
		if (!g_items[i].used) {
			continue;
		}

		if (IsItemStillFaulty(g_items[i])) {
			g_items[i].fault_now = 1u;
			g_items[i].show_until_ms = now_ms + WARNING_SHOW_HOLD_MS;
			continue;
		}

		if (g_items[i].fault_now) {
			g_items[i].fault_now = 0u;
			if (TimeReached(now_ms, g_items[i].show_until_ms)) {
				g_items[i].show_until_ms = now_ms + WARNING_SHOW_HOLD_MS;
			}
		}

		if (TimeReached(now_ms, g_items[i].show_until_ms)) {
			RemoveItemAt(i);
		}
	}
}

/* Подхватывает "старые" активные неисправности без status_changed на старте. */
static void SyncMissingFaultItems(uint32_t now_ms)
{
	/* Подстраховка: если устройство уже пришло в "плохом" состоянии как первое сообщение
	 * (status_changed == 0), всё равно добавляем его в список предупреждений. */
	for (uint8_t mi = 0u; mi < g_active_devices_count; mi++) {
		ActiveDeviceInfo* m = &g_active_devices[mi];
		if (!m->online) {
			continue;
		}
		for (uint8_t vi = 0u; vi < m->vdev_count; vi++) {
			auto* v = &m->vdevs[vi];
			if (!v->online || !IsTrackedVdevType(v->v_d_type)) {
				continue;
			}
			if (!IsFaultLineState(v->line_state)) {
				continue;
			}
			UpsertItem(0u, m->dev.zone, m->dev.h_adr, v->v_l_adr, m->dev.d_type, v->v_d_type, v->line_state, 0u, now_ms);
		}
	}
}

/* Добавляет/снимает предупреждения об обрыве CAN у МКУ по can_status_mask. */
static void SyncMkuCanFaultItems(uint32_t now_ms)
{
	for (uint8_t mi = 0u; mi < g_active_devices_count; mi++) {
		ActiveDeviceInfo* m = &g_active_devices[mi];
		if (!m->online || !m->can_status_valid) {
			continue;
		}
		for (uint8_t can_idx = 1u; can_idx <= 2u; can_idx++) {
			uint8_t bit = (uint8_t)(1u << (can_idx - 1u));
			if ((m->can_status_mask & bit) == 0u) {
				UpsertItem(1u, m->dev.zone, m->dev.h_adr, 0u, m->dev.d_type, 0u, 1u, can_idx, now_ms);
			} else {
				/* Для CAN-линий МКУ снимаем предупреждение сразу при восстановлении. */
				int idx = FindItem(1u, m->dev.zone, m->dev.h_adr, 0u, m->dev.d_type, 0u, can_idx);
				if (idx >= 0) {
					RemoveItemAt((uint8_t)idx);
				}
			}
		}
	}
}

/* Добавляет/снимает предупреждения об отсутствии активности CAN у ППКУ. */
static void SyncPpkuCanFaultItems(uint32_t now_ms)
{
	(void)now_ms;
	for (uint8_t can_idx = 1u; can_idx <= 2u; can_idx++) {
		uint8_t bit = (uint8_t)(1u << (can_idx - 1u));
		if ((can_bus_error_flags & bit) != 0u) {
			UpsertItem(2u, 0u, 0u, 0u, DEVICE_PPKY_TYPE, 0u, 1u, can_idx, now_ms);
		} else {
			/* Для локальной линии ППКУ снимаем предупреждение сразу при восстановлении. */
			int idx = FindItem(2u, 0u, 0u, 0u, DEVICE_PPKY_TYPE, 0u, can_idx);
			if (idx >= 0) {
				RemoveItemAt((uint8_t)idx);
			}
		}
	}
}

/* Формирует отсортированный набор строк для UI (большое/малое поле). */
static uint8_t BuildUiPayload(char (*big_titles)[16], char (*details)[ZONE_NAME_SIZE + 1])
{
	uint8_t order[WARN_MAX_ITEMS];
	uint8_t on = 0u;
	for (uint8_t i = 0u; i < WARN_MAX_ITEMS; i++) {
		if (!g_items[i].used) {
			continue;
		}
		if (!g_items[i].fault_now && TimeReached(HAL_GetTick(), g_items[i].show_until_ms)) {
			continue;
		}
		order[on++] = i;
	}
	/* Стабильный порядок для UI: zone -> h_adr -> l_adr -> type.
	 * Это убирает "скакание" строк при нескольких неисправностях. */
	for (uint8_t a = 1u; a < on; a++) {
		uint8_t key = order[a];
		uint8_t b = a;
		while (b > 0u) {
			const WarningItem& l = g_items[order[b - 1u]];
			const WarningItem& r = g_items[key];
			uint8_t greater = 0u;
			if (l.zone > r.zone) greater = 1u;
			else if (l.zone == r.zone && l.h_adr > r.h_adr) greater = 1u;
			else if (l.zone == r.zone && l.h_adr == r.h_adr && l.v_l_adr > r.v_l_adr) greater = 1u;
			else if (l.zone == r.zone && l.h_adr == r.h_adr && l.v_l_adr == r.v_l_adr && l.kind > r.kind) greater = 1u;
			else if (l.zone == r.zone && l.h_adr == r.h_adr && l.v_l_adr == r.v_l_adr && l.kind == r.kind && l.can_idx > r.can_idx) greater = 1u;
			else if (l.zone == r.zone && l.h_adr == r.h_adr && l.v_l_adr == r.v_l_adr && l.kind == r.kind && l.can_idx == r.can_idx && l.v_d_type > r.v_d_type) greater = 1u;
			if (!greater) {
				break;
			}
			order[b] = order[b - 1u];
			b--;
		}
		order[b] = key;
	}

	uint8_t count = 0u;
	for (uint8_t i = 0u; i < on && count < WARN_MAX_ITEMS; i++) {
		const WarningItem& it = g_items[order[i]];

		if (it.kind == 0u) {
			const char* fault = (it.line_state == 2u) ? "КЗ" : "ОБРЫВ";
			snprintf(big_titles[count], 16, "%s %u", fault, (unsigned)it.v_l_adr);

			char zone_name[ZONE_NAME_SIZE + 1];
			memset(zone_name, 0, sizeof(zone_name));
			if (it.zone > 0u && it.zone <= ZONE_NUMBER) {
				strncpy(zone_name, reinterpret_cast<const char*>(PPKYConfig.zone_name[it.zone - 1u]), ZONE_NAME_SIZE);
				zone_name[ZONE_NAME_SIZE] = '\0';
			}
			if (zone_name[0] == '\0') {
				snprintf(zone_name, sizeof(zone_name), "ЗОНА %u", (unsigned)it.zone);
			}

			snprintf(details[count], ZONE_NAME_SIZE + 1, "%s %s %u",
				 zone_name, McuTypeName(it.mcu_d_type), (unsigned)it.h_adr);
		} else if (it.kind == 1u) {
			snprintf(big_titles[count], 16, "ОБРЫВ");
			snprintf(details[count], ZONE_NAME_SIZE + 1, "%s %u CAN %u",
				 McuTypeName(it.mcu_d_type), (unsigned)it.h_adr, (unsigned)it.can_idx);
		} else {
			snprintf(big_titles[count], 16, "ОБРЫВ");
			snprintf(details[count], ZONE_NAME_SIZE + 1, "ППКУ CAN %u", (unsigned)it.can_idx);
		}
		count++;
	}
	return count;
}

/* Есть ли сейчас хотя бы одна активная (не восстановленная) неисправность. */
static uint8_t HasActiveFaultNow(void)
{
	for (uint8_t i = 0u; i < WARN_MAX_ITEMS; i++) {
		if (g_items[i].used && g_items[i].fault_now) {
			return 1u;
		}
	}
	return 0u;
}

/* Управляет LED_ERR: мгновенное включение, отложенное отключение через 5 с. */
static void UpdateErrorLed(uint32_t now_ms)
{
	if (HasActiveFaultNow()) {
		Led_Set(LED_ERR, 1u);
		g_led_err_on = 1u;
		g_led_err_off_deadline_ms = now_ms + LED_ERR_OFF_DELAY_MS;
		return;
	}

	if (g_led_err_on && TimeReached(now_ms, g_led_err_off_deadline_ms)) {
		Led_Set(LED_ERR, 0u);
		g_led_err_on = 0u;
	}
}

/* Пушит данные в TouchGFX только при реальном изменении (анти-спам). */
static void PushUiIfChanged(uint8_t active, uint8_t count,
			    char (*big_titles)[16], char (*details)[ZONE_NAME_SIZE + 1])
{
	uint8_t same = (g_last_active == active && g_last_count == count) ? 1u : 0u;
	if (same && active) {
		if (memcmp(g_last_big, big_titles, sizeof(g_last_big)) != 0 ||
		    memcmp(g_last_details, details, sizeof(g_last_details)) != 0) {
			same = 0u;
		}
	}
	if (same) {
		return;
	}

	g_last_active = active;
	g_last_count = count;
	memset(g_last_big, 0, sizeof(g_last_big));
	memset(g_last_details, 0, sizeof(g_last_details));
	if (active) {
		memcpy(g_last_big, big_titles, sizeof(g_last_big));
		memcpy(g_last_details, details, sizeof(g_last_details));
	}
	Warning_UiUpdate(active, count, big_titles, details);
}

} // namespace

/* Главный 1мс-тик модуля: сбор, фильтрация, LED и публикация предупреждений. */
void WarningProcess1ms(void)
{
	uint32_t now_ms = HAL_GetTick();
	char big_titles[WARN_MAX_ITEMS][16] = {{0}};
	char details[WARN_MAX_ITEMS][ZONE_NAME_SIZE + 1] = {{0}};

	ConsumeChangedStatuses(now_ms);
	SyncMissingFaultItems(now_ms);
	SyncMkuCanFaultItems(now_ms);
	SyncPpkuCanFaultItems(now_ms);
	PruneInactiveItems(now_ms);
	UpdateErrorLed(now_ms);

	/* Во время пожара предупреждения не отображаем (но список поддерживаем актуальным). */
	if (Fire_IsActive()) {
		PushUiIfChanged(0u, 0u, big_titles, details);
		return;
	}

	uint8_t count = BuildUiPayload(big_titles, details);
	PushUiIfChanged((count > 0u) ? 1u : 0u, count, big_titles, details);
}
