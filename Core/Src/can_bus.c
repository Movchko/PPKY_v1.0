/**
  ******************************************************************************
  * @file    can_bus.c
  * @brief   Реализация приёма CAN по двум закольцованным шинам с дедупликацией по устройствам.
  ******************************************************************************
  */

#include "can_bus.h"
#include "backend.h"
#include "stm32h5xx_hal.h"
#include <string.h>

#define CAN_RX_RING_SIZE      64
#define CAN_NO_RX_TIMEOUT_MS  2000
#define CAN_DUP_WINDOW_MS     20

typedef struct {
	uint32_t id;
	uint8_t  data[8];
	uint8_t  can_bus;
} CanRxEntry;

static CanRxEntry     can_rx_ring[CAN_RX_RING_SIZE];
static volatile uint8_t can_rx_head = 0;
static uint8_t        can_rx_tail  = 0;

static volatile uint32_t last_rx_tick_can1 = 0;
static volatile uint32_t last_rx_tick_can2 = 0;

/** По каждому устройству: последний пакет с CAN1 и CAN2 (для дедупликации). ID = CAN_ID_NONE значит «ещё не было» */
#define CAN_ID_NONE  0xFFFFFFFFu
static uint32_t last_id_can1[CAN_MAX_DEVICES];
static uint8_t  last_data_can1[CAN_MAX_DEVICES][8];
static uint32_t last_id_can2[CAN_MAX_DEVICES];
static uint8_t  last_data_can2[CAN_MAX_DEVICES][8];
static uint8_t  can_init_done = 0;

/** Ожидание дубликата с другой шины: с какой шины ждём, до какого тика */
static uint8_t  pending_bus[CAN_MAX_DEVICES];
static uint32_t pending_timeout[CAN_MAX_DEVICES];

uint8_t can_bus_error_flags = 0;
uint8_t device_can_error[CAN_MAX_DEVICES] = {0};

static void can_rx_push(uint32_t msg_id, const uint8_t *data, uint8_t can_bus)
{
	uint8_t next = can_rx_head + 1;
	if (next >= CAN_RX_RING_SIZE)
		next = 0;
	if (next == can_rx_tail) {
		can_rx_tail++;
		if (can_rx_tail >= CAN_RX_RING_SIZE)
			can_rx_tail = 0;
	}
	can_rx_ring[can_rx_head].id = msg_id;
	memcpy(can_rx_ring[can_rx_head].data, data, 8);
	can_rx_ring[can_rx_head].can_bus = can_bus;
	can_rx_head = next;

	if (can_bus == CAN_BUS_1)
		last_rx_tick_can1 = HAL_GetTick();
	else
		last_rx_tick_can2 = HAL_GetTick();
}

void CanRxPush(uint32_t id, const uint8_t *data, uint8_t can_bus)
{
	can_rx_push(id, data, can_bus);
}

void CanInit(void)
{
	for (uint16_t i = 0; i < CAN_MAX_DEVICES; i++) {
		last_id_can1[i] = CAN_ID_NONE;
		last_id_can2[i] = CAN_ID_NONE;
	}
	can_init_done = 1;
}

void CanProcess(void)
{
	uint32_t now = HAL_GetTick();

	if (!can_init_done)
		CanInit();

	/* Флаги «нет приёма по шине» */
	if (now - last_rx_tick_can1 <= CAN_NO_RX_TIMEOUT_MS)
		can_bus_error_flags &= ~(uint8_t)1;
	else
		can_bus_error_flags |= 1;

	if (now - last_rx_tick_can2 <= CAN_NO_RX_TIMEOUT_MS)
		can_bus_error_flags &= ~(uint8_t)2;
	else
		can_bus_error_flags |= 2;

	/* Таймауты ожидания дубликата по каждому устройству */
	for (uint16_t d = 0; d < CAN_MAX_DEVICES; d++) {
		if (pending_timeout[d] == 0)
			continue;
		if (now <= pending_timeout[d])
			continue;
		device_can_error[d] |= (uint8_t)(1 << (pending_bus[d] - 1));
		pending_timeout[d] = 0;
	}

	while (can_rx_head != can_rx_tail) {
		CanRxEntry *e = &can_rx_ring[can_rx_tail];
		can_rx_tail++;
		if (can_rx_tail >= CAN_RX_RING_SIZE)
			can_rx_tail = 0;

		uint8_t dev;
		uint8_t other_bus;
		uint32_t *last_id_other;
		uint8_t  *last_data_other;
		uint32_t *last_id_cur;
		uint8_t  *last_data_cur;

		dev = CAN_DEVICE_INDEX(e->id);
		if (e->can_bus == CAN_BUS_1)
			other_bus = CAN_BUS_2;
		else
			other_bus = CAN_BUS_1;

		if (other_bus == CAN_BUS_1) {
			last_id_other = &last_id_can1[dev];
			last_data_other = last_data_can1[dev];
		} else {
			last_id_other = &last_id_can2[dev];
			last_data_other = last_data_can2[dev];
		}

		if (e->can_bus == CAN_BUS_1) {
			last_id_cur = &last_id_can1[dev];
			last_data_cur = last_data_can1[dev];
		} else {
			last_id_cur = &last_id_can2[dev];
			last_data_cur = last_data_can2[dev];
		}

		/* Дубликат с другой шины: тот же пакет уже пришёл с другой линии — не парсить, снять ожидание */
		if (*last_id_other != CAN_ID_NONE && e->id == *last_id_other && memcmp(e->data, last_data_other, 8) == 0) {
			*last_id_cur = e->id;
			memcpy(last_data_cur, e->data, 8);
			pending_timeout[dev] = 0;
			device_can_error[dev] &= (uint8_t)(~(1 << (e->can_bus - 1)));
			continue;
		}


		/* Один и тот же пакет дважды с одной шины — пропустить ?????? пока не вижу смысла в пропуске, возможно стоит сигнализировать об этом, т.к это ненормально */
		//if (*last_id_cur != CAN_ID_NONE && e->id == *last_id_cur && memcmp(e->data, last_data_cur, 8) == 0)
		//	continue;

		/* Уникальный пакет: разобрать один раз, ждать дубликат с другой шины */
		ProtocolParse(e->id, e->data);

		*last_id_cur = e->id;
		memcpy(last_data_cur, e->data, 8);

		pending_bus[dev] = other_bus;
		pending_timeout[dev] = now + CAN_DUP_WINDOW_MS;
	}
}