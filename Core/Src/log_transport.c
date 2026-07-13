/*
 * log_transport.c
 */

#include "log_transport.h"
#include "backend.h"
#include "can_bus.h"
#include "main.h"
#include "log_stream.h"

#include <string.h>

#define LOG_TX_QUEUE_SIZE              8u
#define LOG_UART_BODY_MAX              246u

typedef enum {
	LOG_RX_PREAMBLE_0 = 0,
	LOG_RX_PREAMBLE_1,
	LOG_RX_SIZE_LO,
	LOG_RX_SIZE_HI,
	LOG_RX_TYPE_LO,
	LOG_RX_TYPE_HI,
	LOG_RX_SEQ_LO,
	LOG_RX_SEQ_HI,
	LOG_RX_BODY,
	LOG_RX_CRC_LO,
	LOG_RX_CRC_HI
} LogRxState_t;

typedef struct {
	uint8_t data[LOG_STREAM_FRAME_MAX];
	uint16_t len;
	LogTransportPort_t port;
} LogTxEntry_t;

typedef struct {
	LogRxState_t state;
	uint16_t pkt_size;
	uint16_t pkt_type;
	uint16_t pkt_seq;
	uint16_t body_total;
	uint16_t body_pos;
	uint16_t crc_acc;
	uint8_t body_buf[LOG_UART_BODY_MAX];
	uint8_t crc_lo;
} LogRxParser_t;

static LogTxEntry_t s_tx_queue[LOG_TX_QUEUE_SIZE];
static volatile uint8_t s_tx_head = 0u;
static volatile uint8_t s_tx_tail = 0u;

static LogRxParser_t s_uart4_rx;
static uint8_t s_uart4_rx_byte = 0u;
static volatile uint8_t s_uart4_tx_busy = 0u;
static volatile uint8_t s_uart4_rx_started = 0u;

static volatile uint8_t s_uart2_log_tx_busy = 0u;
static uint8_t s_uart2_tx_buf[LOG_STREAM_FRAME_MAX];
static uint16_t s_uart2_tx_len = 0u;

extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart4;

static uint8_t log_tx_ring_next(uint8_t idx)
{
	idx++;
	if (idx >= LOG_TX_QUEUE_SIZE) {
		idx = 0u;
	}
	return idx;
}

static void log_rx_reset(LogRxParser_t *parser)
{
	parser->state = LOG_RX_PREAMBLE_0;
	parser->body_pos = 0u;
}

static void log_transport_try_tx_uart4(void);
static void log_transport_try_tx_uart2(void);
static void log_transport_pump_stream(void);
static void log_transport_handle_log_request(LogTransportPort_t port,
                                             uint16_t seq,
                                             const uint8_t *payload,
                                             uint16_t payload_len);

static uint16_t log_transport_build_frame(uint8_t *out,
                                          uint16_t out_max,
                                          uint16_t pkt_type,
                                          uint16_t seq,
                                          const uint8_t *payload,
                                          uint16_t payload_len)
{
	uint16_t pkt_size;
	uint16_t pos;
	uint16_t crc;

	if (out == NULL || payload_len > LOG_UART_BODY_MAX) {
		return 0u;
	}

	pkt_size = (uint16_t)(BSU_PKT_HEADER_SIZE + payload_len + BSU_PKT_CHECKSUM_SIZE);
	if (pkt_size > out_max || pkt_size > LOG_STREAM_FRAME_MAX) {
		return 0u;
	}

	pos = 0u;
	out[pos++] = BSU_PKT_PREAMBLE_LO;
	out[pos++] = BSU_PKT_PREAMBLE_HI;
	out[pos++] = (uint8_t)(pkt_size & 0xFFu);
	out[pos++] = (uint8_t)(pkt_size >> 8);
	out[pos++] = (uint8_t)(pkt_type & 0xFFu);
	out[pos++] = (uint8_t)(pkt_type >> 8);
	out[pos++] = (uint8_t)(seq & 0xFFu);
	out[pos++] = (uint8_t)(seq >> 8);
	if (payload_len > 0u && payload != NULL) {
		memcpy(&out[pos], payload, payload_len);
		pos = (uint16_t)(pos + payload_len);
	}

	crc = BSU_Checksum(out, pos);
	out[pos++] = (uint8_t)(crc & 0xFFu);
	out[pos++] = (uint8_t)(crc >> 8);
	return pkt_size;
}

static void log_transport_send_cb(LogTransportPort_t port,
                                  uint16_t pkt_type,
                                  uint16_t seq,
                                  const uint8_t *payload,
                                  uint16_t payload_len)
{
	uint8_t next = log_tx_ring_next(s_tx_head);
	LogTxEntry_t *entry;

	if (next == s_tx_tail) {
		s_tx_tail = log_tx_ring_next(s_tx_tail);
	}

	entry = &s_tx_queue[s_tx_head];
	entry->port = port;
	entry->len = log_transport_build_frame(entry->data,
	                                       sizeof(entry->data),
	                                       pkt_type,
	                                       seq,
	                                       payload,
	                                       payload_len);
	if (entry->len == 0u) {
		return;
	}
	s_tx_head = next;
}

static uint8_t uart_bridge_channel_is_free(void)
{
	return UartBridge_IsTxIdle();
}

static void log_transport_uart4_rx_start(void)
{
	if (s_uart4_rx_started != 0u) {
		return;
	}
	if (HAL_UART_Receive_IT(&huart4, &s_uart4_rx_byte, 1u) == HAL_OK) {
		s_uart4_rx_started = 1u;
	}
}

static void log_transport_on_rx_byte(LogRxParser_t *parser,
                                   LogTransportPort_t port,
                                   uint8_t b)
{
	switch (parser->state) {
	case LOG_RX_PREAMBLE_0:
		if (b == BSU_PKT_PREAMBLE_LO) {
			parser->state = LOG_RX_PREAMBLE_1;
		}
		break;
	case LOG_RX_PREAMBLE_1:
		if (b == BSU_PKT_PREAMBLE_HI) {
			parser->state = LOG_RX_SIZE_LO;
			parser->crc_acc = (uint16_t)(BSU_PKT_PREAMBLE_LO + BSU_PKT_PREAMBLE_HI);
		} else {
			log_rx_reset(parser);
		}
		break;
	case LOG_RX_SIZE_LO:
		parser->pkt_size = b;
		parser->crc_acc = (uint16_t)(parser->crc_acc + b);
		parser->state = LOG_RX_SIZE_HI;
		break;
	case LOG_RX_SIZE_HI:
		parser->pkt_size |= (uint16_t)b << 8;
		parser->crc_acc = (uint16_t)(parser->crc_acc + b);
		parser->state = LOG_RX_TYPE_LO;
		break;
	case LOG_RX_TYPE_LO:
		parser->pkt_type = b;
		parser->crc_acc = (uint16_t)(parser->crc_acc + b);
		parser->state = LOG_RX_TYPE_HI;
		break;
	case LOG_RX_TYPE_HI:
		parser->pkt_type |= (uint16_t)b << 8;
		parser->crc_acc = (uint16_t)(parser->crc_acc + b);
		parser->state = LOG_RX_SEQ_LO;
		break;
	case LOG_RX_SEQ_LO:
		parser->pkt_seq = b;
		parser->crc_acc = (uint16_t)(parser->crc_acc + b);
		parser->state = LOG_RX_SEQ_HI;
		break;
	case LOG_RX_SEQ_HI:
		parser->pkt_seq |= (uint16_t)b << 8;
		parser->crc_acc = (uint16_t)(parser->crc_acc + b);
		if (parser->pkt_size < (BSU_PKT_HEADER_SIZE + BSU_PKT_CHECKSUM_SIZE) ||
		    parser->pkt_size > LOG_STREAM_FRAME_MAX) {
			log_rx_reset(parser);
			break;
		}
		parser->body_total = (uint16_t)(parser->pkt_size - BSU_PKT_HEADER_SIZE -
		                                BSU_PKT_CHECKSUM_SIZE);
		if (parser->body_total > LOG_UART_BODY_MAX) {
			log_rx_reset(parser);
			break;
		}
		parser->body_pos = 0u;
		parser->state = LOG_RX_BODY;
		break;
	case LOG_RX_BODY:
		parser->body_buf[parser->body_pos++] = b;
		parser->crc_acc = (uint16_t)(parser->crc_acc + b);
		if (parser->body_pos >= parser->body_total) {
			parser->state = LOG_RX_CRC_LO;
		}
		break;
	case LOG_RX_CRC_LO:
		parser->crc_lo = b;
		parser->state = LOG_RX_CRC_HI;
		break;
	case LOG_RX_CRC_HI: {
		uint16_t recv_crc = (uint16_t)(parser->crc_lo | ((uint16_t)b << 8));
		uint16_t calc_crc = (uint16_t)(parser->crc_acc & 0xFFFFu);

		if (recv_crc == calc_crc && parser->pkt_type == LOG_PKT_TYPE_REQ) {
			log_transport_handle_log_request(port,
			                                 parser->pkt_seq,
			                                 parser->body_buf,
			                                 parser->body_total);
		}
		log_rx_reset(parser);
		break;
	}
	default:
		log_rx_reset(parser);
		break;
	}
}

static void log_transport_pump_stream(void)
{
	uint8_t budget = 16u;

	log_transport_try_tx_uart4();
	log_transport_try_tx_uart2();
	while (budget-- != 0u) {
		if (LogStream_IsDumpActive()) {
			LogStream_Process();
		}
		log_transport_try_tx_uart4();
		log_transport_try_tx_uart2();
		if (!LogStream_IsDumpActive()) {
			break;
		}
	}
}

static void log_transport_handle_log_request(LogTransportPort_t port,
                                             uint16_t seq,
                                             const uint8_t *payload,
                                             uint16_t payload_len)
{
	LogStream_HandleRequest(port, seq, payload, payload_len);
	log_transport_pump_stream();
}

static void log_transport_try_tx_uart4(void)
{
	LogTxEntry_t *entry;

	if (s_uart4_tx_busy != 0u) {
		return;
	}
	while (s_tx_head != s_tx_tail) {
		entry = &s_tx_queue[s_tx_tail];
		if (entry->port != LOG_PORT_UART4 || entry->len == 0u) {
			break;
		}

		HAL_GPIO_WritePin(BRP_485_EN_GPIO_Port, BRP_485_EN_Pin, GPIO_PIN_SET);
		if (HAL_UART_Transmit_IT(&huart4, entry->data, entry->len) == HAL_OK) {
			s_uart4_tx_busy = 1u;
			s_tx_tail = log_tx_ring_next(s_tx_tail);
			return;
		}
		HAL_GPIO_WritePin(BRP_485_EN_GPIO_Port, BRP_485_EN_Pin, GPIO_PIN_RESET);
		break;
	}
}

static void log_transport_try_tx_uart2(void)
{
	LogTxEntry_t *entry;

	if (s_uart2_log_tx_busy != 0u || !uart_bridge_channel_is_free()) {
		return;
	}

	while (s_tx_head != s_tx_tail) {
		entry = &s_tx_queue[s_tx_tail];
		if (entry->port != LOG_PORT_UART2 || entry->len == 0u) {
			if (entry->port == LOG_PORT_UART4) {
				break;
			}
			s_tx_tail = log_tx_ring_next(s_tx_tail);
			continue;
		}

		if (entry->len > sizeof(s_uart2_tx_buf)) {
			s_tx_tail = log_tx_ring_next(s_tx_tail);
			continue;
		}
		memcpy(s_uart2_tx_buf, entry->data, entry->len);
		s_uart2_tx_len = entry->len;
		s_tx_tail = log_tx_ring_next(s_tx_tail);
		if (HAL_UART_Transmit_IT(&huart2, s_uart2_tx_buf, s_uart2_tx_len) == HAL_OK) {
			s_uart2_log_tx_busy = 1u;
		}
		return;
	}
}

void LogTransport_Init(void)
{
	s_tx_head = 0u;
	s_tx_tail = 0u;
	s_uart4_tx_busy = 0u;
	s_uart2_log_tx_busy = 0u;
	log_rx_reset(&s_uart4_rx);
	LogStream_Init(log_transport_send_cb);
	log_transport_uart4_rx_start();
}

void LogTransport_Process(void)
{
	uint8_t budget = 4u;

	log_transport_try_tx_uart4();
	log_transport_try_tx_uart2();
	while (budget-- != 0u && LogStream_IsDumpActive()) {
		uint8_t next = log_tx_ring_next(s_tx_head);
		if (next == s_tx_tail) {
			break;
		}
		LogStream_Process();
	}
	log_transport_try_tx_uart4();
	log_transport_try_tx_uart2();
}

void LogTransport_OnUart2LogRequest(uint16_t seq,
                                    const uint8_t *payload,
                                    uint16_t payload_len)
{
	log_transport_handle_log_request(LOG_PORT_UART2, seq, payload, payload_len);
}

void LogTransport_OnUartRxByte(UART_HandleTypeDef *huart)
{
	if (huart == &huart4) {
		log_transport_on_rx_byte(&s_uart4_rx, LOG_PORT_UART4, s_uart4_rx_byte);
		(void)HAL_UART_Receive_IT(&huart4, &s_uart4_rx_byte, 1u);
	}
}

void LogTransport_OnUartTxComplete(UART_HandleTypeDef *huart)
{
	if (huart == &huart4) {
		s_uart4_tx_busy = 0u;
		HAL_GPIO_WritePin(BRP_485_EN_GPIO_Port, BRP_485_EN_Pin, GPIO_PIN_RESET);
		log_transport_try_tx_uart4();
	} else if (huart == &huart2) {
		if (s_uart2_log_tx_busy != 0u) {
			s_uart2_log_tx_busy = 0u;
			log_transport_try_tx_uart2();
		}
	}
}

void LogTransport_OnUartError(UART_HandleTypeDef *huart)
{
	if (huart == &huart4) {
		s_uart4_tx_busy = 0u;
		HAL_GPIO_WritePin(BRP_485_EN_GPIO_Port, BRP_485_EN_Pin, GPIO_PIN_RESET);
		s_uart4_rx_started = 0u;
		log_transport_uart4_rx_start();
	} else if (huart == &huart2) {
		s_uart2_log_tx_busy = 0u;
	}
}
