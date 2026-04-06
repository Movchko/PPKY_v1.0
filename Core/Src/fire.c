#include "fire.h"

#include "main.h"
#include "button.h"
#include "beeper.h"
#include "led.h"
#include "backend.h"
#include "device_config.h"
#include <string.h>
#include <stdio.h>
#include "app.hpp"

extern PPKYCfg PPKYConfig;
extern ActiveDeviceInfo g_active_devices[32];
extern uint8_t g_active_devices_count;

/* Отладка: зона 1 → индекс 0; пока 3 зоны, далее — из конфига МКУ */
#define FIRE_DEBUG_ZONES     3u
#define FIRE_MAX_SLOTS       16u
#define FIRE_UI_MAX_ZONES    16u
#define FIRE_UI_NAME_LEN     (ZONE_NAME_SIZE + 1u)
/* Дежурный режим после обработки пожара: короткий писк раз в N секунд */
#define FIRE_DUTY_BEEP_PERIOD_MS 5000u

static uint8_t debug_zone_delay[FIRE_DEBUG_ZONES] = { 15, 30u, 30 };
static uint8_t debug_module_delay[FIRE_DEBUG_ZONES][2] = {
	{ 0u, 0u },
	{ 0u, 5u },
	{ 5u, 10u },
};

typedef enum {
	FIRE_STATE_IDLE = 0,
	FIRE_STATE_WAIT_AUTO,
	FIRE_STATE_WAIT_MANUAL,
	FIRE_STATE_EXTINGUISHING
} FireState;

typedef enum {
	FIRE_EVENT_NONE = 0,
	FIRE_EVENT_STATUS_FIRE,
	FIRE_EVENT_REPLY_FIRE,
	FIRE_EVENT_STOP_EXT,
	FIRE_EVENT_BTN_START_SP,
	FIRE_EVENT_BTN_START_ALL,
	FIRE_EVENT_BTN_STOP,
	FIRE_EVENT_TICK_1MS
} FireEvent;

typedef struct {
	uint8_t  zone;
	uint8_t  active;
	uint8_t  phase2_sent;
	uint32_t phase2_deadline_ms;
} FireZoneSlot;

/* Центральный runtime-контекст сценария пожара (FSM + индикация + UI/звук). */
typedef struct {
	FireState state;
	uint8_t   reply_received;
	uint8_t   all_hold_active;
	uint16_t  all_hold_ms;
	uint32_t  state_start_ms;
	uint32_t  led_toggle_ms;
	uint8_t   beeper_alert_active;
	uint8_t   beeper_duty_active;
	uint32_t  beeper_duty_last_ms;
	uint8_t   stop_launch_pressed_latched;
	uint8_t   start_launch_pressed_latched;
	uint8_t   led_fire_on;
	uint8_t   btn_start_all_hold_latched;
	uint8_t   btn_start_sp_latched;
	uint8_t   btn_stop_latched;
	uint8_t   start_all_is_bright;
	uint8_t   last_ui_active;
	uint8_t   last_ui_remaining;
	uint8_t   last_ui_nzones;
	uint32_t  last_ui_force_names_ms;
	char      last_ui_names[FIRE_UI_MAX_ZONES][FIRE_UI_NAME_LEN];
	/* После «ОСТАНОВ ПУСКА»: авто-отсчёт зон и его отображение отключены, слоты пожара сохраняются */
	uint8_t   zone_countdown_stopped;
	FireZoneSlot slots[FIRE_MAX_SLOTS];
} FireContext;

static FireContext g_fire;

static void Fire_SetStartAllBrightness(uint8_t bright);
static void Fire_SendStopAllMcus(void);
static void Fire_SendPhase1Zone(uint8_t zone);
static void Fire_SendPhase2Zone(uint8_t zone);
static void Fire_Phase2AllPending(void);
static void Fire_ClearAllSlots(void);
static void Fire_SyncStateFromSlots(void);
static uint8_t Fire_DebugZoneIndex(uint8_t zone);
static uint8_t Fire_ZoneDelaySec(uint8_t zone);
static void Fire_GetModuleDelays(uint8_t zone, uint8_t *m0, uint8_t *m1);
static int8_t Fire_FindSlotZone(uint8_t zone);
static int8_t Fire_AllocSlot(void);
/** @return 1 если зона добавлена впервые в цикле, 0 если по этой зоне пожар уже учтён */
static uint8_t Fire_TryAddNewFireZone(uint8_t zone, uint32_t now_ms);
static uint8_t Fire_AnyActiveSlot(void);
static uint8_t Fire_CountPendingPhase2(void);
static uint8_t Fire_MinRemainingSec(uint32_t now_ms);
static uint8_t Fire_ProcessAutoDeadlines(uint32_t now_ms);
static uint8_t Fire_StartAllExistingZonesAndMarkSlots(void);
static void Fire_FillZoneNamesForUi(char (*out_names)[FIRE_UI_NAME_LEN], uint8_t *out_n);
static uint8_t Fire_CollectSortedIgniterIndices(uint8_t zone, uint8_t *out_idx, uint8_t max_out);
static void Fire_BeeperEnterAlert(void);
static void Fire_BeeperEnterDuty(uint32_t now_ms);
static void Fire_BeeperDutyTick(uint32_t now_ms);

extern void Fire_UiUpdate(uint8_t active, uint8_t remaining_s, uint8_t n_zones,
			  char (*zone_names)[FIRE_UI_NAME_LEN]);

static void Fire_BeeperEnterAlert(void)
{
	/* Сигнальный режим: непрерывный звук до обработки пожара. */
	g_fire.beeper_alert_active = 1u;
	g_fire.beeper_duty_active = 0u;
	Beeper_ContinuousOn();
}

static void Fire_BeeperEnterDuty(uint32_t now_ms)
{
	/* Дежурный режим: короткие напоминания по таймеру (без непрерывного звука). */
	g_fire.beeper_alert_active = 0u;
	g_fire.beeper_duty_active = 1u;
	Beeper_ContinuousOff();
	g_fire.beeper_duty_last_ms = now_ms;
}

static void Fire_BeeperDutyTick(uint32_t now_ms)
{
	if (!g_fire.beeper_duty_active) {
		return;
	}
	if ((now_ms - g_fire.beeper_duty_last_ms) >= FIRE_DUTY_BEEP_PERIOD_MS) {
		Beeper_ShortBeep();
		g_fire.beeper_duty_last_ms = now_ms;
	}
}

static uint8_t Fire_DebugZoneIndex(uint8_t zone)
{
	if (zone < FIRE_DEBUG_ZONES) {
		return zone;
	}
	return FIRE_DEBUG_ZONES - 1u;
}

static uint8_t Fire_ZoneDelaySec(uint8_t zone)
{
	return debug_zone_delay[Fire_DebugZoneIndex(zone)];
}

static void Fire_GetModuleDelays(uint8_t zone, uint8_t *m0, uint8_t *m1)
{
	uint8_t z = Fire_DebugZoneIndex(zone);
	*m0 = debug_module_delay[z][0];
	*m1 = debug_module_delay[z][1];
}

static uint8_t Fire_CollectSortedIgniterIndices(uint8_t zone, uint8_t *out_idx, uint8_t max_out)
{
	/* Список МКУ igniter в зоне, отсортированный по h_adr для стабильного порядка команд. */
	uint8_t n = 0u;
	for (uint8_t i = 0u; i < g_active_devices_count && n < max_out; i++) {
		if (g_active_devices[i].dev.zone != zone) {
			continue;
		}
		if ((g_active_devices[i].dev.d_type & 0x7Fu) != DEVICE_MCU_IGN_TYPE) {
			continue;
		}
		out_idx[n++] = i;
	}
	for (uint8_t a = 1u; a < n; a++) {
		uint8_t key = out_idx[a];
		uint8_t kh = g_active_devices[key].dev.h_adr;
		uint8_t b = a;
		while (b > 0u && g_active_devices[out_idx[b - 1u]].dev.h_adr > kh) {
			out_idx[b] = out_idx[b - 1u];
			b--;
		}
		out_idx[b] = key;
	}
	return n;
}

static void Fire_SendStartToIgniterIdx(uint8_t idx, uint8_t zone, uint8_t zd_sec, uint8_t md_sec)
{
	const ActiveDeviceInfo *ad = &g_active_devices[idx];
	can_ext_id_t can_id;
	uint8_t data[8] = { 0 };

	can_id.ID = 0;
	can_id.field.dir = 0;
	can_id.field.d_type = ad->dev.d_type & 0x7Fu;
	can_id.field.h_adr = ad->dev.h_adr;
	can_id.field.l_adr = ad->dev.l_adr & 0x3Fu;
	can_id.field.zone = ad->dev.zone & 0x7Fu;

	data[0] = (uint8_t)ServiceCmd_StartExtinguishment;
	data[1] = zone;
	data[2] = zd_sec;
	data[3] = md_sec;
	SendMessageFull(can_id, data, 0, BUS_CAN12);
}

static void Fire_SendPhase1Zone(uint8_t zone)
{
	/* Фаза 1: старт в зоне с zone_delay + module_delay (igniter[0..1]). */
	uint8_t ign[8];
	uint8_t n = Fire_CollectSortedIgniterIndices(zone, ign, 8u);
	uint8_t zd = Fire_ZoneDelaySec(zone);
	uint8_t m0, m1;
	Fire_GetModuleDelays(zone, &m0, &m1);
	if (n >= 1u) {
		Fire_SendStartToIgniterIdx(ign[0], zone, zd, m0);
	}
	if (n >= 2u) {
		Fire_SendStartToIgniterIdx(ign[1], zone, zd, m1);
	}
}

static void Fire_SendPhase2Zone(uint8_t zone)
{
	/* Фаза 2: немедленный старт (zone_delay=0), учитываем только module_delay. */
	uint8_t ign[8];
	uint8_t n = Fire_CollectSortedIgniterIndices(zone, ign, 8u);
	uint8_t m0, m1;
	Fire_GetModuleDelays(zone, &m0, &m1);
	if (n >= 1u) {
		Fire_SendStartToIgniterIdx(ign[0], zone, 0u, m0);
	}
	if (n >= 2u) {
		Fire_SendStartToIgniterIdx(ign[1], zone, 0u, m1);
	}
}

static void Fire_SendStopAllMcus(void)
{
	/* Массовая остановка по всем обнаруженным МКУ типов пожарного контура. */
	can_ext_id_t can_id;
	uint8_t data[8] = { 0 };
	data[0] = (uint8_t)ServiceCmd_StopExtinguishment;

	for (uint8_t i = 0u; i < g_active_devices_count; i++) {
		uint8_t t = g_active_devices[i].dev.d_type & 0x7Fu;
		if (t != DEVICE_MCU_IGN_TYPE && t != DEVICE_MCU_TC_TYPE &&
		    t != DEVICE_MCU_K1 && t != DEVICE_MCU_K2 &&
		    t != DEVICE_MCU_K3 && t != DEVICE_MCU_KR) {
			continue;
		}
		can_id.ID = 0;
		can_id.field.dir = 0;
		can_id.field.d_type = t;
		can_id.field.h_adr = g_active_devices[i].dev.h_adr;
		can_id.field.l_adr = g_active_devices[i].dev.l_adr & 0x3Fu;
		can_id.field.zone = g_active_devices[i].dev.zone & 0x7Fu;
		SendMessageFull(can_id, data, 0, BUS_CAN12);
	}
}

static int8_t Fire_FindSlotZone(uint8_t zone)
{
	for (uint8_t i = 0u; i < FIRE_MAX_SLOTS; i++) {
		if (g_fire.slots[i].active && g_fire.slots[i].zone == zone) {
			return (int8_t)i;
		}
	}
	return -1;
}

static int8_t Fire_AllocSlot(void)
{
	for (uint8_t i = 0u; i < FIRE_MAX_SLOTS; i++) {
		if (!g_fire.slots[i].active) {
			return (int8_t)i;
		}
	}
	return -1;
}

static uint8_t Fire_TryAddNewFireZone(uint8_t zone, uint32_t now_ms)
{
	/* Добавляет новую пожарную зону в слот и сразу отправляет фазу 1.
	 * Повтор по той же зоне в текущем цикле — игнорируем. */
	if (Fire_FindSlotZone(zone) >= 0) {
		/* За один рабочий цикл повторного пожара по той же зоне не бывает — дубль игнорируем */
		return 0u;
	}
	int8_t si = Fire_AllocSlot();
	if (si < 0) {
		si = 0;
	}
	FireZoneSlot *s = &g_fire.slots[(uint8_t)si];
	s->active = 1u;
	s->zone = zone;
	s->phase2_sent = 0u;
	s->phase2_deadline_ms = now_ms + (uint32_t)Fire_ZoneDelaySec(zone) * 1000u;
	Fire_SendPhase1Zone(zone);
	return 1u;
}

static void Fire_ClearAllSlots(void)
{
	memset(g_fire.slots, 0, sizeof(g_fire.slots));
	g_fire.zone_countdown_stopped = 0u;
}

static uint8_t Fire_AnyActiveSlot(void)
{
	for (uint8_t i = 0u; i < FIRE_MAX_SLOTS; i++) {
		if (g_fire.slots[i].active) {
			return 1u;
		}
	}
	return 0u;
}

static uint8_t Fire_CountPendingPhase2(void)
{
	uint8_t c = 0u;
	for (uint8_t i = 0u; i < FIRE_MAX_SLOTS; i++) {
		if (g_fire.slots[i].active && !g_fire.slots[i].phase2_sent) {
			c++;
		}
	}
	return c;
}

static uint8_t Fire_MinRemainingSec(uint32_t now_ms)
{
	if (g_fire.zone_countdown_stopped) {
		return 0u;
	}
	uint32_t best_ms = 0xFFFFFFFFu;
	uint8_t found = 0u;
	for (uint8_t i = 0u; i < FIRE_MAX_SLOTS; i++) {
		if (!g_fire.slots[i].active || g_fire.slots[i].phase2_sent) {
			continue;
		}
		found = 1u;
		if (now_ms >= g_fire.slots[i].phase2_deadline_ms) {
			return 0u;
		}
		uint32_t rem = g_fire.slots[i].phase2_deadline_ms - now_ms;
		if (rem < best_ms) {
			best_ms = rem;
		}
	}
	if (!found) {
		return 0u;
	}
	return (uint8_t)((best_ms + 999u) / 1000u);
}

static uint8_t Fire_ProcessAutoDeadlines(uint32_t now_ms)
{
	/* Автозапуск фазы 2 по дедлайнам только в WAIT_AUTO и только для pending-слотов. */
	uint8_t any_started = 0u;
	if (g_fire.zone_countdown_stopped) {
		return 0u;
	}
	if (g_fire.state != FIRE_STATE_WAIT_AUTO) {
		return 0u;
	}
	for (uint8_t i = 0u; i < FIRE_MAX_SLOTS; i++) {
		if (!g_fire.slots[i].active || g_fire.slots[i].phase2_sent) {
			continue;
		}
		if (now_ms >= g_fire.slots[i].phase2_deadline_ms) {
			Fire_SendPhase2Zone(g_fire.slots[i].zone);
			g_fire.slots[i].phase2_sent = 1u;
			any_started = 1u;
		}
	}
	return any_started;
}

static void Fire_Phase2AllPending(void)
{
	for (uint8_t i = 0u; i < FIRE_MAX_SLOTS; i++) {
		if (!g_fire.slots[i].active || g_fire.slots[i].phase2_sent) {
			continue;
		}
		Fire_SendPhase2Zone(g_fire.slots[i].zone);
		g_fire.slots[i].phase2_sent = 1u;
	}
}

static uint8_t Fire_StartAllExistingZonesAndMarkSlots(void)
{
	/* ПУСК ОБЩИЙ без привязки к статусу ПОЖАРА:
	 * отправка фазы 2 во все существующие зоны igniter и синхронизация слотов. */
	uint8_t zone_sent[128] = {0};
	uint8_t any_started = 0u;

	for (uint8_t i = 0u; i < g_active_devices_count; i++) {
		uint8_t t = g_active_devices[i].dev.d_type & 0x7Fu;
		if (t != DEVICE_MCU_IGN_TYPE) {
			continue;
		}
		uint8_t z = g_active_devices[i].dev.zone & 0x7Fu;
		if (zone_sent[z]) {
			continue;
		}
		zone_sent[z] = 1u;
		Fire_SendPhase2Zone(z);
		any_started = 1u;
	}

	/* Для слотов пожара помечаем отправку фазы 2 только в реально запущенные зоны */
	for (uint8_t i = 0u; i < FIRE_MAX_SLOTS; i++) {
		if (!g_fire.slots[i].active || g_fire.slots[i].phase2_sent) {
			continue;
		}
		if (zone_sent[g_fire.slots[i].zone & 0x7Fu]) {
			g_fire.slots[i].phase2_sent = 1u;
		}
	}

	return any_started;
}

static void Fire_SyncStateFromSlots(void)
{
	/* Синхронизирует верхнеуровневое состояние FSM из фактического состояния слотов. */
	if (!Fire_AnyActiveSlot()) {
		g_fire.state = FIRE_STATE_IDLE;
		return;
	}
	if (Fire_CountPendingPhase2() == 0u) {
		g_fire.state = FIRE_STATE_EXTINGUISHING;
	} else if (g_fire.state == FIRE_STATE_EXTINGUISHING || g_fire.state == FIRE_STATE_IDLE) {
		g_fire.state = (PPKYConfig.fire_mode == 2u) ? FIRE_STATE_WAIT_MANUAL : FIRE_STATE_WAIT_AUTO;
	}
}

static void Fire_FillZoneNamesForUi(char (*out_names)[FIRE_UI_NAME_LEN], uint8_t *out_n)
{
	/* Готовит уникальный отсортированный список имён зон для TouchGFX. */
	uint8_t zones[FIRE_MAX_SLOTS];
	uint8_t nz = 0u;

	for (uint8_t i = 0u; i < FIRE_MAX_SLOTS; i++) {
		if (!g_fire.slots[i].active) {
			continue;
		}
		uint8_t z = g_fire.slots[i].zone;
		uint8_t dup = 0u;
		for (uint8_t j = 0u; j < nz; j++) {
			if (zones[j] == z) {
				dup = 1u;
				break;
			}
		}
		if (!dup && nz < FIRE_MAX_SLOTS) {
			zones[nz++] = z;
		}
	}
	for (uint8_t a = 1u; a < nz; a++) {
		uint8_t key = zones[a];
		uint8_t b = a;
		while (b > 0u && zones[b - 1u] > key) {
			zones[b] = zones[b - 1u];
			b--;
		}
		zones[b] = key;
	}

	if (nz > FIRE_UI_MAX_ZONES) {
		nz = FIRE_UI_MAX_ZONES;
	}
	*out_n = nz;
	for (uint8_t i = 0u; i < nz; i++) {
		uint8_t zi = zones[i];
		char *dst = out_names[i];
		if (zi >= ZONE_NUMBER) {
			dst[0] = '\0';
			continue;
		}
		char name[ZONE_NAME_SIZE + 1u];
		memcpy(name, PPKYConfig.zone_name[zi], ZONE_NAME_SIZE);
		name[ZONE_NAME_SIZE] = '\0';
		for (int s = (int)ZONE_NAME_SIZE - 1; s >= 0; s--) {
			if (name[s] == ' ' || name[s] == '\0') {
				name[s] = '\0';
			} else {
				break;
			}
		}
		if (name[0] == '\0') {
			(void)snprintf(dst, (size_t)FIRE_UI_NAME_LEN, "Зона %u", (unsigned)zi + 1u);
		} else {
			(void)snprintf(dst, (size_t)FIRE_UI_NAME_LEN, "%s", name);
		}
		dst[FIRE_UI_NAME_LEN - 1u] = '\0';
	}
}

static void Fire_UpdateUiText(uint8_t active, uint8_t remaining_s, uint8_t n_zones,
			      char (*zone_names)[FIRE_UI_NAME_LEN])
{
	/* Пуш в UI только при изменениях; есть защита от редкой рассинхронизации n_zones==0. */
	uint8_t same = (g_fire.last_ui_active == active && g_fire.last_ui_remaining == remaining_s &&
			g_fire.last_ui_nzones == n_zones);
	if (same && n_zones > 0u) {
		same = (memcmp(g_fire.last_ui_names, zone_names,
			       (size_t)n_zones * (size_t)FIRE_UI_NAME_LEN) == 0);
	}
	/*
	 * Раньше при n_zones==0 кэш считал одинаковым (active, remaining, 0) и годами не вызывал
	 * Fire_UiUpdate, пока не сменится секунда таймера — имя зоны не доходило до TouchGFX.
	 * При активных слотах список имён должен быть непуст: периодически пробиваем кэш.
	 */
	if (same && active && n_zones == 0u && Fire_AnyActiveSlot()) {
		uint32_t t = HAL_GetTick();
		if ((t - g_fire.last_ui_force_names_ms) >= 50u) {
			g_fire.last_ui_force_names_ms = t;
			same = 0u;
		}
	}
	if (same) {
		return;
	}
	g_fire.last_ui_active = active;
	g_fire.last_ui_remaining = remaining_s;
	g_fire.last_ui_nzones = n_zones;
	if (n_zones > 0u) {
		memcpy(g_fire.last_ui_names, zone_names,
		       (size_t)n_zones * (size_t)FIRE_UI_NAME_LEN);
	}
	Fire_UiUpdate(active, remaining_s, n_zones, zone_names);
}

static void Fire_SetIdleIndication(void)
{
	/* Полный дежурный профиль индикаторов/звука в состоянии IDLE. */
	Led_Set(LED_BUT_START_SP, 0);
	Led_Set(LED_STR_START_SP, 0);
	Led_Set(LED_BUT_START_ALL, 0);
	Fire_SetStartAllBrightness(0u);
	Led_Set(LED_STR_START_ALL, 1u);
	Led_Set(LED_BUT_STOP, 0);
	Led_Set(LED_STR_STOP, 0);
	Led_Set(LED_START, 0);
	Led_Set(LED_STOP, 0);
	Led_Set(LED_AUTO_OFF, 0);
	Led_Set(LED_FIRE, 0);
	g_fire.stop_launch_pressed_latched = 0u;
	g_fire.beeper_alert_active = 0u;
	g_fire.beeper_duty_active = 0u;
	Beeper_ContinuousOff();
}

static void Fire_SetStartAllBrightness(uint8_t bright)
{
	if (g_fire.start_all_is_bright == bright) {
		return;
	}
	g_fire.start_all_is_bright = bright;
	uint8_t pwr = bright ? LED_BUT_MAX_BRIGHTNESS : LED_BUT_DIM_BRIGHTNESS;
	Led_SetBrightness(LED_BUT_START_ALL, pwr);
	Led_SetBrightness(LED_STR_START_ALL, pwr);
}

static uint8_t Fire_ButtonPressedEvent(uint8_t button_id, uint8_t *latched_flag)
{
	ButtonState st = Button_GetState(button_id);
	if ((st == ButtonStatePress || st == ButtonStateLongPress) && (*latched_flag == 0u)) {
		*latched_flag = 1u;
		return 1u;
	}
	if (st == ButtonStateReset) {
		*latched_flag = 0u;
	}
	return 0u;
}

static void Fire_ApplyStateLeds(void)
{
	/* Профиль индикации для не-IDLE состояний; отдельные override применяются в Fire_Transition(). */
	if (PPKYConfig.fire_mode == 2u) {
		Led_Set(LED_AUTO_OFF, 1);
	} else {
		Led_Set(LED_AUTO_OFF, 0);
	}

	uint8_t pending = Fire_CountPendingPhase2();

	if (pending > 0u) {
		Led_Set(LED_BUT_START_SP, 1);
		Led_Set(LED_STR_START_SP, 1);
		Led_Set(LED_BUT_STOP, 1);
		Led_Set(LED_STR_STOP, 1);
		if (g_fire.all_hold_active) {
			Fire_SetStartAllBrightness(1u);
			Led_Set(LED_BUT_START_ALL, 1u);
		} else {
			Fire_SetStartAllBrightness(0u);
			Led_Set(LED_BUT_START_ALL, 0u);
			Led_Set(LED_STR_START_ALL, 1u);
		}
		Led_Set(LED_START, 0);
	} else {
		Led_Set(LED_BUT_START_SP, 0);
		Led_Set(LED_STR_START_SP, 0);
		Led_Set(LED_BUT_STOP, 0);
		Led_Set(LED_STR_STOP, 0);
		Fire_SetStartAllBrightness(1u);
		Led_Set(LED_BUT_START_ALL, 1);
		Led_Set(LED_STR_START_ALL, 1);
		Led_Set(LED_START, 1);
	}
	Led_Set(LED_STOP, g_fire.stop_launch_pressed_latched ? 1u : 0u);
}

static void Fire_Transition(FireEvent ev, uint32_t now_ms)
{
	/* Единая точка обработки событий сценария пожара:
	 * FSM, автодедлайны, звук, LED и публикация состояния на UI. */
	uint8_t ui_active = 0u;
	uint8_t ui_remaining = 0u;
	uint8_t fire_processed = 0u;

	switch (ev) {
	case FIRE_EVENT_STATUS_FIRE:
		g_fire.start_launch_pressed_latched = 0u;
		if (g_fire.state == FIRE_STATE_IDLE) {
			g_fire.state = (PPKYConfig.fire_mode == 2u) ? FIRE_STATE_WAIT_MANUAL : FIRE_STATE_WAIT_AUTO;
			g_fire.reply_received = 0u;
		}
		/* Новый пожар всегда переводит пищалку в сигнальный непрерывный режим */
		Fire_BeeperEnterAlert();
		break;
	case FIRE_EVENT_REPLY_FIRE:
		g_fire.reply_received = 1u;
		break;
	case FIRE_EVENT_STOP_EXT:
		/* Команда StopExtinguishment с CAN: только остановка таймеров/автопуска, без сброса слотов */
		Fire_SendStopAllMcus();
		g_fire.zone_countdown_stopped = 1u;
		g_fire.start_launch_pressed_latched = 0u;
		g_fire.all_hold_active = 0u;
		g_fire.all_hold_ms = 0u;
		g_fire.btn_start_all_hold_latched = 0u;
		if (Fire_CountPendingPhase2() > 0u) {
			g_fire.state = (PPKYConfig.fire_mode == 2u) ? FIRE_STATE_WAIT_MANUAL : FIRE_STATE_WAIT_AUTO;
		}
		break;
	case FIRE_EVENT_BTN_START_SP:
		/* ПУСК СП: только при активных слотах пожара и есть зоны без фазы 2 */
		if (!Fire_AnyActiveSlot() || Fire_CountPendingPhase2() == 0u) {
			break;
		}
		if (g_fire.state == FIRE_STATE_WAIT_AUTO || g_fire.state == FIRE_STATE_WAIT_MANUAL ||
		    g_fire.state == FIRE_STATE_EXTINGUISHING) {
			/* Пуск тушения обработан — индикацию «ОСТАНОВ ПУСКА» снимаем */
			g_fire.start_launch_pressed_latched = 1u;
			g_fire.stop_launch_pressed_latched = 0u;
			Fire_Phase2AllPending();
			Fire_SyncStateFromSlots();
			fire_processed = 1u;
		}
		break;
	case FIRE_EVENT_BTN_START_ALL:
		/* ПУСК ОБЩИЙ: запуск тушения всех существующих зон с module_delay (zone_delay=0),
		 * независимо от статуса ПОЖАРА/слотов */
		if (Fire_StartAllExistingZonesAndMarkSlots()) {
			g_fire.start_launch_pressed_latched = 1u;
			g_fire.stop_launch_pressed_latched = 0u;
			Fire_SyncStateFromSlots();
			fire_processed = 1u;
		}
		break;
	case FIRE_EVENT_BTN_STOP:
		/* ОСТАНОВ ПУСКА: ручной режим, стоп МКУ, отображение/авто-таймеры зон off, пожар и слоты сохраняются */
		if (g_fire.start_launch_pressed_latched || Fire_CountPendingPhase2() == 0u) {
			break;
		}
		PPKYConfig.fire_mode = 2u;
		g_fire.zone_countdown_stopped = 1u;
		g_fire.start_launch_pressed_latched = 0u;
		g_fire.stop_launch_pressed_latched = 1u;
		Fire_SendStopAllMcus();
		g_fire.all_hold_active = 0u;
		g_fire.all_hold_ms = 0u;
		g_fire.btn_start_all_hold_latched = 0u;
		if (Fire_CountPendingPhase2() > 0u) {
			g_fire.state = FIRE_STATE_WAIT_MANUAL;
		}
		fire_processed = 1u;
		break;
	case FIRE_EVENT_TICK_1MS:
	default:
		break;
	}

	if (Fire_ProcessAutoDeadlines(now_ms)) {
		g_fire.start_launch_pressed_latched = 1u;
		g_fire.stop_launch_pressed_latched = 0u;
		fire_processed = 1u;
	}
	Fire_SyncStateFromSlots();
	if (fire_processed && g_fire.state != FIRE_STATE_IDLE) {
		/* Пожар обработан (стоп/пуск/автопуск) -> дежурный режим пищалки */
		Fire_BeeperEnterDuty(now_ms);
	}
	if (g_fire.beeper_alert_active) {
		/* Поддерживаем непрерывный режим даже если кто-то прервал его другим паттерном */
		Beeper_ContinuousOn();
	} else {
		Fire_BeeperDutyTick(now_ms);
	}

	if (g_fire.all_hold_active && g_fire.all_hold_ms < 3000u) {
		uint32_t rem_ms = 3000u - g_fire.all_hold_ms;
		ui_remaining = (uint8_t)((rem_ms + 999u) / 1000u);
	} else if (g_fire.state == FIRE_STATE_WAIT_AUTO || g_fire.state == FIRE_STATE_WAIT_MANUAL) {
		if (!g_fire.zone_countdown_stopped) {
			ui_remaining = Fire_MinRemainingSec(now_ms);
		} else {
			ui_remaining = 0u;
		}
	} else if (g_fire.state == FIRE_STATE_EXTINGUISHING) {
		ui_remaining = 0u;
	}

	if (g_fire.state == FIRE_STATE_IDLE) {
		Led_Set(LED_FIRE, 0);
		g_fire.led_fire_on = 0u;
	} else {
		if ((now_ms - g_fire.led_toggle_ms) >= 500u) {
			g_fire.led_toggle_ms = now_ms;
			g_fire.led_fire_on = !g_fire.led_fire_on;
			Led_Set(LED_FIRE, g_fire.led_fire_on ? 1u : 0u);
		}
	}

	if (g_fire.state == FIRE_STATE_IDLE) {
		if (g_fire.all_hold_active && g_fire.all_hold_ms < 3000u) {
			/* Без пожара: показываем только 3-сек таймер удержания ПУСК ОБЩИЙ и мигание подписи */
			Fire_SetIdleIndication();
			{
				uint8_t blink_on = (((now_ms / 500u) & 1u) != 0u) ? 1u : 0u;
				Led_Set(LED_BUT_START_ALL, 1u);
				Led_Set(LED_STR_START_ALL, blink_on);
			}
			{
				char z0[FIRE_UI_MAX_ZONES][FIRE_UI_NAME_LEN];
				Fire_UpdateUiText(1u, ui_remaining, 0u, z0);
			}
			return;
		}
		Fire_SetIdleIndication();
		if (g_fire.start_launch_pressed_latched) {
			Led_Set(LED_START, 1u);
		}
		{
			char z0[FIRE_UI_MAX_ZONES][FIRE_UI_NAME_LEN];
			Fire_UpdateUiText(0u, 0u, 0u, z0);
		}
		return;
	}

	Fire_ApplyStateLeds();
	if ((g_fire.state == FIRE_STATE_WAIT_AUTO || g_fire.state == FIRE_STATE_WAIT_MANUAL) &&
	    g_fire.all_hold_active) {
		uint8_t blink_on = (((now_ms / 500u) & 1u) != 0u) ? 1u : 0u;
		Led_Set(LED_BUT_START_ALL, 1u);
		Led_Set(LED_STR_START_ALL, blink_on);
	}

	ui_active = 1u;
	{
		char zn[FIRE_UI_MAX_ZONES][FIRE_UI_NAME_LEN];
		uint8_t nzn = 0u;
		Fire_FillZoneNamesForUi(zn, &nzn);
		Fire_UpdateUiText(ui_active, ui_remaining, nzn, zn);
	}
}

void Fire_Init(void)
{
	/* Инициализация контекста пожара; полный сброс слотов только при старте/перезапуске. */
	memset(&g_fire, 0, sizeof(g_fire));
	/* Полный сброс слотов только при перезапуске ППКУ */
	Fire_ClearAllSlots();
	g_fire.state = FIRE_STATE_IDLE;
	g_fire.start_all_is_bright = 0xFFu;
	Fire_SetStartAllBrightness(0u);
	Led_Set(LED_BUT_START_ALL, 0u);
	Led_Set(LED_STR_START_ALL, 1u);
	g_fire.start_all_is_bright = 0u;
	g_fire.last_ui_nzones = 0u;
}

void Fire_Timer1ms(void)
{
	/* 1мс-путь: крутит FSM при активном сценарии или удержании ПУСК ОБЩИЙ. */
	uint32_t now = HAL_GetTick();
	if (g_fire.state == FIRE_STATE_IDLE && !Fire_AnyActiveSlot() && !g_fire.all_hold_active) {
		return;
	}
	Fire_Transition(FIRE_EVENT_TICK_1MS, now);
}

void Fire_Timer10ms(void)
{
	/* 10мс-путь: кнопки (в т.ч. удержание ПУСК ОБЩИЙ 3с) и edge-trigger событий. */
	ButtonState st_start_all = Button_GetState(BUT_FORCE);
	if (st_start_all == ButtonStatePress || st_start_all == ButtonStateLongPress) {
		if (!g_fire.all_hold_active) {
			g_fire.all_hold_active = 1u;
			g_fire.all_hold_ms = 0u;
			g_fire.state_start_ms = HAL_GetTick();
		} else {
			if (g_fire.all_hold_ms < 3000u) {
				g_fire.all_hold_ms += 10u;
			}
			if (g_fire.all_hold_ms >= 3000u && g_fire.btn_start_all_hold_latched == 0u) {
				g_fire.btn_start_all_hold_latched = 1u;
				Fire_Transition(FIRE_EVENT_BTN_START_ALL, HAL_GetTick());
			}
		}
	} else if (st_start_all == ButtonStateReset) {
		g_fire.all_hold_active = 0u;
		g_fire.all_hold_ms = 0u;
		g_fire.btn_start_all_hold_latched = 0u;
		/* Обновить UI/LED сразу после отпускания кнопки, даже если пожара нет */
		Fire_Transition(FIRE_EVENT_TICK_1MS, HAL_GetTick());
	}

	if (g_fire.state == FIRE_STATE_IDLE && !Fire_AnyActiveSlot()) {
		return;
	}

	if (Fire_ButtonPressedEvent(BUT_FIRE, &g_fire.btn_start_sp_latched)) {
		Fire_Transition(FIRE_EVENT_BTN_START_SP, HAL_GetTick());
	}
	if (Fire_ButtonPressedEvent(BUT_STOP, &g_fire.btn_stop_latched)) {
		Fire_Transition(FIRE_EVENT_BTN_STOP, HAL_GetTick());
	}
}

void Fire_OnStatusFire(uint32_t msg_id)
{
	/* Вход статуса пожара от МКУ: зона -> слот -> фаза 1 -> событие FSM + ReplyStatusFire. */
	can_ext_id_t id;
	id.ID = msg_id & 0x0FFFFFFF;
	uint8_t zone = (uint8_t)(id.field.zone & 0x7Fu);
	if (zone != 0u) {
		zone--;
	}
	uint32_t now = HAL_GetTick();
	if (Fire_TryAddNewFireZone(zone, now)) {
		/* Новая зона: слот, фаза 1, затем FSM — UI видит все активные зоны */
		Fire_Transition(FIRE_EVENT_STATUS_FIRE, now);
	}
	SetReplyStatusFire(zone);
}

void Fire_OnReplyStatusFire(uint32_t msg_id)
{
	(void)msg_id;
	Fire_Transition(FIRE_EVENT_REPLY_FIRE, HAL_GetTick());
}

void Fire_OnStopExtinguishment(uint32_t msg_id)
{
	(void)msg_id;
	Fire_Transition(FIRE_EVENT_STOP_EXT, HAL_GetTick());
}
