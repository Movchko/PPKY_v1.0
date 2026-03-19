#include "fire.h"

#include "main.h"
#include "button.h"
#include "beeper.h"
#include "led.h"
#include "backend.h"
#include "device_config.h"
#include <string.h>

/* Внешние объекты из app.cpp / backend */
extern PPKYCfg PPKYConfig;
extern Device BoardDevicesList[];
extern uint8_t nDevs;

typedef enum {
	FIRE_STATE_IDLE = 0,
	FIRE_STATE_WAIT_AUTO,      /* ждём 30с (режим AUTO/AUTONOMOUS) */
	FIRE_STATE_WAIT_MANUAL,    /* пожар активен, ждём ручного пуска */
	FIRE_STATE_EXTINGUISHING   /* команда тушения отправлена */
} FireState;

typedef enum {
	FIRE_EVENT_NONE = 0,
	FIRE_EVENT_STATUS_FIRE,
	FIRE_EVENT_REPLY_FIRE,
	FIRE_EVENT_STOP_EXT,
	FIRE_EVENT_BTN_FORCE,
	FIRE_EVENT_BTN_FIRE,
	FIRE_EVENT_BTN_STOP,
	FIRE_EVENT_TICK_1MS
} FireEvent;

typedef struct {
	FireState state;
	uint8_t   zone;              /* зона пожара */
	uint8_t   reply_received;    /* пришёл ReplyStatusFire */

	/* Удержание ПУСК СП (3с) */
	uint8_t   sp_hold_active;
	uint16_t  sp_hold_ms;

	uint32_t  state_start_ms;    /* старт текущего состояния (для авто-таймера) */
	uint32_t  led_toggle_ms;     /* последний тик мигания LED_FIRE */

	uint8_t   beeper_on;
	uint8_t   led_fire_on;

	uint8_t   btn_force_latched;
	uint8_t   btn_start_all_latched;
	uint8_t   btn_stop_latched;

	/* кэш UI чтобы не спамить обновления без изменений */
	uint8_t   last_ui_active;
	uint8_t   last_ui_zone;
	uint8_t   last_ui_remaining;
} FireContext;

static FireContext g_fire;

static void Fire_SendStartForZone(uint8_t zone);
static void Fire_SendStartAllZones(void);

extern void Fire_UiUpdate(uint8_t active, uint8_t zone, uint8_t remaining_s);

static void Fire_UpdateUiText(uint8_t active, uint8_t zone, uint8_t remaining_s)
{
	if (g_fire.last_ui_active == active &&
	    g_fire.last_ui_zone == zone &&
	    g_fire.last_ui_remaining == remaining_s) {
		return;
	}
	g_fire.last_ui_active = active;
	g_fire.last_ui_zone = zone;
	g_fire.last_ui_remaining = remaining_s;
	Fire_UiUpdate(active, zone, remaining_s);
}

static void Fire_SetIdleIndication(void)
{
	Led_Set(LED_BUT_START_SP, 0);
	Led_Set(LED_STR_START_SP, 0);
	Led_Set(LED_BUT_START_ALL, 0);
	Led_Set(LED_STR_START_ALL, 0);
	Led_Set(LED_BUT_STOP, 0);
	Led_Set(LED_STR_STOP, 0);
	Led_Set(LED_START, 0);
	Led_Set(LED_STOP, 0);
	Led_Set(LED_AUTO_OFF, 0);
	Led_Set(LED_FIRE, 0);
	if (g_fire.beeper_on) {
		Beeper_ContinuousOff();
		g_fire.beeper_on = 0;
	}
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
	if (PPKYConfig.fire_mode == 2u) {
		Led_Set(LED_AUTO_OFF, 1);
	} else {
		Led_Set(LED_AUTO_OFF, 0);
	}

	switch (g_fire.state) {
	case FIRE_STATE_WAIT_AUTO:
	case FIRE_STATE_WAIT_MANUAL:
		/* ПУСК СП: всегда подсвечен, при удержании надпись мигает ниже */
		Led_Set(LED_BUT_START_SP, 1);
		Led_Set(LED_STR_START_SP, 1);
		Led_Set(LED_BUT_STOP, 1);
		Led_Set(LED_STR_STOP, 1);
		Led_Set(LED_BUT_START_ALL, 0);
		Led_Set(LED_STR_START_ALL, 0);
		Led_Set(LED_START, 0);
		Led_Set(LED_STOP, 0);
		break;
	case FIRE_STATE_EXTINGUISHING:
		Led_Set(LED_BUT_START_SP, 0);
		Led_Set(LED_STR_START_SP, 0);
		Led_Set(LED_BUT_STOP, 0);
		Led_Set(LED_STR_STOP, 0);
		Led_Set(LED_BUT_START_ALL, 1);
		Led_Set(LED_STR_START_ALL, 1);
		Led_Set(LED_START, 1);
		Led_Set(LED_STOP, 0);
		break;
	case FIRE_STATE_IDLE:
	default:
		break;
	}
}

static void Fire_Transition(FireEvent ev, uint32_t now_ms)
{
	uint8_t ui_active = 0;
	uint8_t ui_zone = 0xFF;
	uint8_t ui_remaining = 0;

	switch (ev) {
	case FIRE_EVENT_STATUS_FIRE:
		/* Старт пожара */
		if (g_fire.state == FIRE_STATE_IDLE) {
			g_fire.state = (PPKYConfig.fire_mode == 2u) ? FIRE_STATE_WAIT_MANUAL : FIRE_STATE_WAIT_AUTO;
			g_fire.state_start_ms = now_ms;
			g_fire.reply_received = 0;
			if (!g_fire.beeper_on) {
				Beeper_ContinuousOn();
				g_fire.beeper_on = 1;
			}
		}
		break;
	case FIRE_EVENT_REPLY_FIRE:
		g_fire.reply_received = 1u;
		break;
	case FIRE_EVENT_STOP_EXT:
		/* Останов тушения считаем завершением сценария пожара */
		g_fire.state = FIRE_STATE_IDLE;
		break;
	case FIRE_EVENT_BTN_FORCE:
		/* Реальный запуск ПУСК СП – только после удержания 3с (см. Fire_Timer10ms) */
		if (g_fire.state == FIRE_STATE_WAIT_AUTO || g_fire.state == FIRE_STATE_WAIT_MANUAL) {
			if (g_fire.zone != 0xFFu) {
				Fire_SendStartForZone(g_fire.zone);
				g_fire.state = FIRE_STATE_EXTINGUISHING;
			}
		}
		break;
	case FIRE_EVENT_BTN_FIRE:
		if (g_fire.state == FIRE_STATE_WAIT_AUTO || g_fire.state == FIRE_STATE_WAIT_MANUAL) {
			Fire_SendStartAllZones();
			g_fire.state = FIRE_STATE_EXTINGUISHING;
		}
		break;
	case FIRE_EVENT_BTN_STOP:
		/* После подтверждения от МКУ останов кнопкой блокируем */
		if (!g_fire.reply_received && PPKYConfig.fire_mode != 2u) {
			PPKYConfig.fire_mode = 2u;
			g_fire.state = FIRE_STATE_WAIT_MANUAL;
		}
		break;
	case FIRE_EVENT_TICK_1MS:
	default:
		break;
	}

	/* Авто-таймер в режиме WAIT_AUTO */
	if (g_fire.state == FIRE_STATE_WAIT_AUTO && g_fire.zone != 0xFFu) {
		uint32_t elapsed_ms = now_ms - g_fire.state_start_ms;
		if (elapsed_ms >= 30000u) {
			Fire_SendStartForZone(g_fire.zone);
			g_fire.state = FIRE_STATE_EXTINGUISHING;
		} else {
			uint32_t rem_ms = 30000u - elapsed_ms;
			ui_remaining = (uint8_t)(rem_ms / 1000u);
		}
	}

	/* Если удерживаем ПУСК СП (до старта) — на экране показываем отсчёт 3..1 */
	if ((g_fire.state == FIRE_STATE_WAIT_AUTO || g_fire.state == FIRE_STATE_WAIT_MANUAL) &&
	    g_fire.sp_hold_active && g_fire.sp_hold_ms < 3000u) {
		uint32_t rem_ms = 3000u - g_fire.sp_hold_ms;
		ui_remaining = (uint8_t)((rem_ms + 999u) / 1000u); /* ceil */
	}

	/* Индикация FIRE LED */
	if (g_fire.state == FIRE_STATE_IDLE) {
		Led_Set(LED_FIRE, 0);
		g_fire.led_fire_on = 0;
	} else {
		if (g_fire.reply_received) {
			g_fire.led_fire_on = 1u;
			Led_Set(LED_FIRE, 1u);
		} else if ((now_ms - g_fire.led_toggle_ms) >= 500u) {
			g_fire.led_toggle_ms = now_ms;
			g_fire.led_fire_on = !g_fire.led_fire_on;
			Led_Set(LED_FIRE, g_fire.led_fire_on ? 1u : 0u);
		}
	}

	/* Индикация по состояниям */
	if (g_fire.state == FIRE_STATE_IDLE) {
		Fire_SetIdleIndication();
	} else {
		Fire_ApplyStateLeds();
		/* Во время удержания ПУСК СП мигает надпись 0.5 Гц */
		if ((g_fire.state == FIRE_STATE_WAIT_AUTO || g_fire.state == FIRE_STATE_WAIT_MANUAL) &&
		    g_fire.sp_hold_active) {
			uint8_t blink_on = (((now_ms / 500u) & 1u) != 0u) ? 1u : 0u;
			Led_Set(LED_STR_START_SP, blink_on);
		}
		ui_active = 1u;
		ui_zone = g_fire.zone;
	}

	Fire_UpdateUiText(ui_active, ui_zone, ui_remaining);
}

static void Fire_SendStartForZone(uint8_t zone)
{
	can_ext_id_t can_id;
	uint8_t data[8] = {0};

	data[0] = ServiceCmd_StartExtinguishment;
	data[1] = zone;

	for (uint8_t i = 0; i < nDevs; i++) {
		uint8_t d_type = BoardDevicesList[i].d_type;
		if (d_type != DEVICE_MCU_IGN_TYPE && d_type != DEVICE_MCU_TC_TYPE)
			continue;
		if (BoardDevicesList[i].zone != zone)
			continue;

		can_id.ID = 0;
		can_id.field.dir   = 0; /* вниз к устройствам */
		can_id.field.d_type = d_type & 0x7Fu;
		can_id.field.h_adr = BoardDevicesList[i].h_adr;
		can_id.field.l_adr = BoardDevicesList[i].l_adr & 0x3Fu;
		can_id.field.zone  = BoardDevicesList[i].zone & 0x7Fu;

		SendMessageFull(can_id, data, SEND_NOW, BUS_CAN12);
	}
}

static void Fire_SendStartAllZones(void)
{
	uint8_t seen_zones[ZONE_NUMBER] = {0};

	for (uint8_t i = 0; i < nDevs; i++) {
		uint8_t d_type = BoardDevicesList[i].d_type;
		if (d_type != DEVICE_MCU_IGN_TYPE && d_type != DEVICE_MCU_TC_TYPE)
			continue;

		uint8_t zone = BoardDevicesList[i].zone;
		if (zone >= ZONE_NUMBER)
			continue;
		if (seen_zones[zone])
			continue;
		seen_zones[zone] = 1;
		Fire_SendStartForZone(zone);
	}
}

void Fire_Init(void)
{
	memset(&g_fire, 0, sizeof(g_fire));
	g_fire.state = FIRE_STATE_IDLE;
	g_fire.zone = 0xFFu;
}

void Fire_Timer1ms(void)
{
	uint32_t now = HAL_GetTick();
	if (g_fire.state == FIRE_STATE_IDLE)
		return;
	Fire_Transition(FIRE_EVENT_TICK_1MS, now);
}

void Fire_Timer10ms(void)
{
	/* Кнопки тушения обрабатываем только если пожар уже зафиксирован */
	if (g_fire.state == FIRE_STATE_IDLE)
		return;

	/* ПУСК СП: логика удержания 3 секунды с обратным отсчётом.
	 * В твоей прошивке кнопка, которая соответствует "ПУСК СП", сейчас читается как BUT_FIRE. */
	ButtonState st_force = Button_GetState(BUT_FIRE);
	if (st_force == ButtonStatePress || st_force == ButtonStateLongPress) {
		if (!g_fire.sp_hold_active) {
			g_fire.sp_hold_active = 1u;
			g_fire.sp_hold_ms = 0u;
			g_fire.state_start_ms = HAL_GetTick(); /* для синхронизации UI-обратного отсчёта */
		} else {
			if (g_fire.sp_hold_ms < 3000u) {
				g_fire.sp_hold_ms += 10u; /* шаг 10 мс */
			}
			if (g_fire.sp_hold_ms >= 3000u) {
				/* Удержание выполнено — генерируем событие BTN_FORCE один раз */
				if (g_fire.btn_force_latched == 0u) {
					g_fire.btn_force_latched = 1u;
					Fire_Transition(FIRE_EVENT_BTN_FORCE, HAL_GetTick());
				}
			}
		}
	} else {
		/* Отпустили ПУСК СП до 3с – сбрасываем счётчик и флаг */
		g_fire.sp_hold_active = 0u;
		g_fire.sp_hold_ms = 0u;
		g_fire.btn_force_latched = 0u;
	}

	/* Обновляем индикацию/текст для удержания кнопки ПУСК СП */
	Fire_Transition(FIRE_EVENT_TICK_1MS, HAL_GetTick());
	/* ПУСК Общий: сразу, кнопка BUT_FORCE */
	if (Fire_ButtonPressedEvent(BUT_FORCE, &g_fire.btn_start_all_latched)) {
		Fire_Transition(FIRE_EVENT_BTN_FIRE, HAL_GetTick());
	}
	if (Fire_ButtonPressedEvent(BUT_STOP, &g_fire.btn_stop_latched)) {
		Fire_Transition(FIRE_EVENT_BTN_STOP, HAL_GetTick());
	}
}

void Fire_OnStatusFire(uint32_t msg_id)
{
	can_ext_id_t id;
	id.ID = msg_id & 0x0FFFFFFF;
	g_fire.zone = (uint8_t)(id.field.zone & 0x7Fu);
	Fire_Transition(FIRE_EVENT_STATUS_FIRE, HAL_GetTick());
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

