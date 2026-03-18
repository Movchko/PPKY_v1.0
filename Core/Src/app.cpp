#include "power_control.h"
#include "app.hpp"
#include "button.h"
#include "beeper.h"
#include "device_config.h"
#include "led.h"
#include "backend.h"
#include "gui/common/FrontendHeap.hpp"



struct PPKYCfg PPKYConfig;       // локальная (рабочая) конфигурация
struct PPKYCfg SavedPPKYConfig; // копия сохранённой конфигурации из Flash

extern SPIF_HandleTypeDef hFlash;

/*
 * fire param
 *
 *
 *
 */
struct s_fire {
	uint8_t isfire;
	RTC_TimeTypeDef t;
	RTC_DateTypeDef d;
	uint8_t zone;
	can_ext_id_t dev;
};

s_fire fire;

/*
 * end fire param
 */


PControl *Power[2];

typedef struct {
	Device dev;
	uint32_t last_seen_ms;
	uint8_t online;
} ActiveDeviceInfo;

static ActiveDeviceInfo g_active_devices[32];
static uint8_t g_active_devices_count = 0;
static uint8_t g_mku_mismatch_flag = 0;

/* --- Механизм автоматической установки адресов по команде 10 --- */
typedef enum {
	ADDR_AUTO_IDLE = 0,
	ADDR_AUTO_WAIT_AFTER_STOP,
	ADDR_AUTO_WAIT_AFTER_SET
} AddrAutoState;

static AddrAutoState g_addr_auto_state = ADDR_AUTO_IDLE;
static uint32_t g_addr_auto_phase_start_ms = 0;



GPIO_TypeDef   *POWER_ST_PORT[2] = {ST1_MK_GPIO_Port, ST2_MK_GPIO_Port};
uint16_t  		POWER_ST_PIN[2] = {ST1_MK_Pin, ST2_MK_Pin};
GPIO_TypeDef   *POWER_OUT_PORT[2] = {KEY_1_GPIO_Port, KEY_2_GPIO_Port};
uint16_t  		POWER_OUT_PIN[2] = {KEY_1_Pin, KEY_2_Pin};

bool isAppInit = 0;



extern int32_t CHANNEL_VAL[NUM_ADC_CHANNEL];

uint8_t status_sec_cnt = 0;

uint8_t uart_send_buf[32] = {0};

extern UART_HandleTypeDef huart2;

static void AddrAuto_ClearActiveDevices(void) {
	memset(g_active_devices, 0, sizeof(g_active_devices));
	g_active_devices_count = 0;
	g_mku_mismatch_flag = 0;
}

static void AddrAuto_Start(void) {
	// Широковещательно: остановить ретрансляцию на CAN0
	uint8_t data[7] = {0};
	data[0] = 1u; // 1 = стоп
	SendAllMessage(ServiceCmd_StopStartReTranslate, data, SEND_NOW, BUS_CAN12);

	g_addr_auto_state = ADDR_AUTO_WAIT_AFTER_STOP;
	g_addr_auto_phase_start_ms = HAL_GetTick();
}

static void AddrAuto_Process(uint32_t now_ms) {
	switch (g_addr_auto_state) {
	case ADDR_AUTO_IDLE:
		break;
	case ADDR_AUTO_WAIT_AFTER_STOP:
		// ждём 100 мс после остановки ретрансляции, затем шлём CircSetAdr
		if ((now_ms - g_addr_auto_phase_start_ms) >= 200u) {
			uint8_t data[7] = {0};
			data[0] = 1u; // новый адрес = 1
			SendAllMessage(ServiceCmd_CircSetAdr, data, SEND_NOW, BUS_CAN0);

			g_addr_auto_state = ADDR_AUTO_WAIT_AFTER_SET;
			g_addr_auto_phase_start_ms = now_ms;
		}
		break;
	case ADDR_AUTO_WAIT_AFTER_SET:
		// ещё 100 мс, потом включаем ретрансляцию, очищаем список устройств
		// и перезапускаем питание на обоих каналах
		if ((now_ms - g_addr_auto_phase_start_ms) >= 500u) {
			//uint8_t data[7] = {0};
			//data[0] = 0u; // 0 = старт ретрансляции
			//SendAllMessage(ServiceCmd_StopStartReTranslate, data, SEND_NOW, BUS_CAN12);

			// адреса изменились — очищаем список активных устройств, он будет заполнен заново
			AddrAuto_ClearActiveDevices();

			// Перезапустить питание на обоих каналах (короткое выключение/включение)
			for (uint8_t i = 0; i < 2; i++) {
				if (Power[i] != nullptr) {
					Power[i]->PControlSetOut(i, false);
				}
			}
			HAL_Delay(500);
			for (uint8_t i = 0; i < 2; i++) {
				if (Power[i] != nullptr) {
					Power[i]->PControlSetOut(i, true);
				}
			}

			g_addr_auto_state = ADDR_AUTO_IDLE;
		}
		break;
	}
}


void USBSendData(uint8_t *Buf) {};

static void SaveSystemStateFromActiveDevices(void)
{
	/* Заполняем PPKYConfig.CfgDevices по текущим активным МКУ на шине.
	 * Пока сохраняем только Device (devId) без конфигураций МКУ и виртуальных устройств. */
	memset(PPKYConfig.CfgDevices, 0, sizeof(PPKYConfig.CfgDevices));

	uint8_t out_i = 0;
	for (uint8_t i = 0; i < g_active_devices_count && out_i < 32; i++) {
		if (g_active_devices[i].online == 0u) {
			continue;
		}
		PPKYConfig.CfgDevices[out_i].UId.devId = g_active_devices[i].dev;
		out_i++;
	}

	/* После сохранения списка считаем, что флаг несовпадения можно пересчитать */
	g_mku_mismatch_flag = 0;
	SaveConfig();
}

void PPKY_GetLastPowerOnDate(RTC_DateTypeDef *out_date, RTC_TimeTypeDef *out_time)
{
	uint32_t v = HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR1);

	/* Формат BKP-регистра: 0xMMDDHHmm (BCD) */
	if (out_date != nullptr) {
		RTC_DateTypeDef d = {};
		d.Month = (uint8_t)((v >> 24) & 0xFFu);  // BCD-месяц
		d.Date  = (uint8_t)((v >> 16) & 0xFFu);  // BCD-день
		// Год и день недели не сохраняем
		*out_date = d;
	}

	if (out_time != nullptr) {
		RTC_TimeTypeDef t = {};
		t.Hours   = (uint8_t)((v >> 8)  & 0xFFu);  // BCD-часы
		t.Minutes = (uint8_t)((v >> 0)  & 0xFFu);  // BCD-минуты
		// Секунды не сохраняем
		*out_time = t;
	}
}

void CommandCB(uint8_t Dev, uint8_t Command, uint8_t *Parameters) {
	(void)Dev;
	(void)Parameters;
	switch(Command) {
	case 10: {
		// Запуск механизма установки адресов (работаем только по CAN0).
		// Механизм неблокирующий: шаги выполняются в AddrAuto_Process() по таймеру.
		if (g_addr_auto_state == ADDR_AUTO_IDLE) {
			AddrAuto_Start();
		}
	}break;
	case 11: {
		/* Сохранить состояние системы: список найденных МКУ на шине */
		SaveSystemStateFromActiveDevices();
	}break;

	default: break;
	}


}



static void UartSendPpkyTime(void) {
	// Формат: "PPKY " + 6 цифр BCD (HHMMSS) + "\r\n"
	// Системное время берём из RTC
	RTC_TimeTypeDef now_time;
	if (HAL_RTC_GetTime(&hrtc, &now_time, RTC_FORMAT_BCD) != HAL_OK) {
		return;
	}

	// BCD поля RTC: 0x23 → "23"
	uint8_t h = now_time.Hours;
	uint8_t m = now_time.Minutes;
	uint8_t s = now_time.Seconds;

	uint8_t buf[16];
	buf[0] = 'P';
	buf[1] = 'P';
	buf[2] = 'K';
	buf[3] = 'Y';
	buf[4] = ' ';
	buf[5] = ((h >> 4) & 0x0F) + '0';
	buf[6] = (h & 0x0F) + '0';
	buf[7] = ((m >> 4) & 0x0F) + '0';
	buf[8] = (m & 0x0F) + '0';
	buf[9] = ((s >> 4) & 0x0F) + '0';
	buf[10] = (s & 0x0F) + '0';
	buf[11] = '\r';
	buf[12] = '\n';

	// Копируем в глобальный uart_send_buf и шлём по USART2 раз в секунду
	memset(uart_send_buf, 0, sizeof(uart_send_buf));
	memcpy(uart_send_buf, buf, 13);
	HAL_UART_Transmit(&huart2, uart_send_buf, 13, 10);
}

void RcvStatusFire() {

}
void RcvReplyStatusFire(){}
void RcvStartExtinguishment(){}
void RcvStopExtinguishment(){}

void AppSetStatus() {

	uint8_t power = (CHANNEL_VAL[4] / 100) & 0xFF; // шаг 100мВ (198 равно 19.8В)
	uint8_t Rpower = (CHANNEL_VAL[0] / 100) & 0xFF;
	uint8_t current1 = (CHANNEL_VAL[1] / 50) & 0xFF; // шаг 50мА
	uint8_t current2 = (CHANNEL_VAL[2] / 50) & 0xFF;
	uint8_t status_data[7] = {
			status_sec_cnt,
			power,
			Rpower,
			current1,
			current2,
			0,
			0
	};
	/* Dev=0 — сама плата ППКУ, отправляем через backend */
	SendMessage(0, 0, status_data, SEND_NOW, BUS_CAN12);

	// Параллельно раз в секунду шлём время ППКУ по UART
	UartSendPpkyTime();
}

static void UpdateActiveDeviceList(uint32_t msg_id, uint32_t now_ms) {
	can_ext_id_t id;
	id.ID = msg_id;
	// интересуют только устройства МКУ (13, 14) и посылки dir=1
	if (id.field.dir == 0)
		return;
	if (id.field.d_type != DEVICE_MCU_IGN_TYPE && id.field.d_type != DEVICE_MCU_TC_TYPE)
		return;

	Device dev;
	dev.zone  = (uint8_t)(id.field.zone & 0x7Fu);
	dev.h_adr = (uint8_t)id.field.h_adr;
	dev.l_adr = (uint8_t)(id.field.l_adr & 0x3Fu);
	dev.d_type = (uint8_t)id.field.d_type;

	// поиск уже известного
	for (uint8_t i = 0; i < g_active_devices_count; i++) {
		if (g_active_devices[i].dev.zone  == dev.zone &&
		    g_active_devices[i].dev.h_adr == dev.h_adr &&
		    g_active_devices[i].dev.l_adr == dev.l_adr &&
		    g_active_devices[i].dev.d_type == dev.d_type) {
			g_active_devices[i].last_seen_ms = now_ms;
			g_active_devices[i].online = 1;
			return;
		}
	}

	if (g_active_devices_count < 32) {
		g_active_devices[g_active_devices_count].dev = dev;
		g_active_devices[g_active_devices_count].last_seen_ms = now_ms;
		g_active_devices[g_active_devices_count].online = 1;
		g_active_devices_count++;
	}
}

static void RefreshActiveDevices(uint32_t now_ms) {
	for (uint8_t i = 0; i < g_active_devices_count; i++) {
		if (g_active_devices[i].online &&
		    (now_ms - g_active_devices[i].last_seen_ms) > 5000u) {
			g_active_devices[i].online = 0;
		}
	}
}

static void CheckMkuConfigMismatch(void) {
	// Сравнить активные онлайн-устройства с конфигом PPKYConfig.CfgDevices
	g_mku_mismatch_flag = 0;

	for (uint8_t i = 0; i < g_active_devices_count; i++) {
		if (!g_active_devices[i].online)
			continue;

		uint8_t found = 0;
		for (uint8_t j = 0; j < 32u; j++) {
			const MKUCfg *m = &PPKYConfig.CfgDevices[j];
			const Device *dv = &m->UId.devId;
			if (dv->d_type == 0)
				continue;
			if (dv->zone  == g_active_devices[i].dev.zone &&
			    dv->h_adr == g_active_devices[i].dev.h_adr &&
			    dv->l_adr == g_active_devices[i].dev.l_adr &&
			    dv->d_type == g_active_devices[i].dev.d_type) {
				found = 1;
				break;
			}
		}
		if (!found) {
			g_mku_mismatch_flag = 1;
			break;
		}
	}
}

void SetHAdr(uint8_t h_adr) {
	extern Device BoardDevicesList[];
	PPKYConfig.UId.devId.h_adr = h_adr;
	BoardDevicesList[0].h_adr = h_adr;
	SaveConfig();
}


uint8_t PControlGetSTCB(uint8_t ch) {
	uint8_t st = 0;
	st = HAL_GPIO_ReadPin(POWER_ST_PORT[ch], POWER_ST_PIN[ch]);
	return st;
}

uint32_t PControlGetADCCB(uint8_t ch) {
	// ch = 0 → ток канала 1, ch = 1 → ток канала 2
	// CHANNEL_VAL[1], CHANNEL_VAL[2] — токи в мА (или код АЦП/пересчитанное значение)
	switch (ch) {
	case 0:
		return CHANNEL_VAL[1];
	case 1:
		return CHANNEL_VAL[2];
	default:
		return 0u;
	}
}

void PControlSetOutCB(uint8_t ch, uint8_t out) {
	HAL_GPIO_WritePin(POWER_OUT_PORT[ch], POWER_OUT_PIN[ch], (GPIO_PinState)out);
}

void AppInit() {

	// Чтение сохранённой конфигурации из Flash (область конфигурации)
	uint32_t cfg_addr = SPIF_SectorToAddress(FLASH_CFG_START_SECTOR);
	PPKYConfigHeader hdr;

	//SPIF_EraseChip(&hFlash);
/*
	for (uint32_t s = 0; s < FLASH_CFG_SECTORS_USED; s++) {
		SPIF_EraseSector(&hFlash, FLASH_CFG_START_SECTOR + s);
	}
*/
	SPIF_ReadAddress(&hFlash, cfg_addr, (uint8_t *)&hdr, sizeof(hdr));

	bool header_ok = (hdr.magic == PPKY_CFG_HEADER_MAGIC) &&
			         (hdr.size  == sizeof(PPKYConfig));

	//ReadSavedConfig();

	if (header_ok) {


		ReadSavedConfig();
/*
		// Заголовок валиден — читаем полезную часть
		SPIF_ReadAddress(&hFlash,
				         cfg_addr + sizeof(PPKYConfigHeader),
						 (uint8_t *)&SavedPPKYConfig,
						 sizeof(SavedPPKYConfig));
						 */
		PPKYConfig = SavedPPKYConfig;
	} else {
		// Заголовок мусор: считаем, что конфигурации нет
		// Сбрасываем на значения по умолчанию и сохраняем в область конфигурации
		//DefaultConfig();

		FillConfigTemplate();

		SaveConfig();
	}

	//FillConfigTemplate();

	//SaveConfig();

	// Передаём указатели в backend (для сервисных команд работы с конфигурацией)
	SetConfigPtr((uint8_t *)&SavedPPKYConfig, (uint8_t *)&PPKYConfig);

	// Список устройств по аналогии с МКУ: 0-й элемент — сама плата ППКУ
	extern Device BoardDevicesList[];
	extern uint8_t nDevs;

	if(PPKYConfig.UId.devId.h_adr == 0) PPKYConfig.UId.devId.h_adr = 1;

	nDevs = 1; /* Dev 0 — ППКУ */
	BoardDevicesList[0].zone  = PPKYConfig.UId.devId.zone & 0x7Fu;
	BoardDevicesList[0].h_adr = PPKYConfig.UId.devId.h_adr;
	BoardDevicesList[0].l_adr = PPKYConfig.UId.devId.l_adr & 0x3Fu;
	BoardDevicesList[0].d_type = DEVICE_PPKY_TYPE;

	Button_Init();
	Beeper_Init();

	// Сообщаем модели, какую функцию вызывать при смене состояния звука
	FrontendHeap::getInstance().model.setSoundToggledCallback(Beeper_SoundOnOff);

	for(uint8_t i = 0; i < 2; i++) {
		Power[i] = new PControl(i);
		Power[i]->PControlInit(PControlGetSTCB, PControlGetADCCB, PControlSetOutCB);
	}
/*
	HAL_Delay(1000);
	Power[0]->PControlSetOut(0, true);
	HAL_Delay(1000);
	Power[1]->PControlSetOut(1, true);
	*/
	isAppInit = true;
	extern bool isListener;
	isListener = true;
	extern uint8_t isMaster;
	isMaster = 1;
}

volatile uint8_t set[2] = {1, 1};
volatile uint8_t st[2];
extern "C" void PControl_OnStatusFault(uint8_t ch, uint32_t now_ms) {
	if (ch < 2 && Power[ch] != nullptr) {
		Power[ch]->OnStatusFault(now_ms);
	}
}

void AppProcess(uint32_t now_ms) {
	if (isAppInit == false)
		return;
	for (uint8_t i = 0; i < 2; i++) {
		if (Power[i] == nullptr)
			continue;
		Power[i]->SetEnable(set[i] != 0);
		Power[i]->Process(now_ms);
		st[i] = Power[i]->PControlGetST(i);
	}
	// Неблокирующая машина состояний автозадания адресов по команде 10
	AddrAuto_Process(now_ms);
}
uint32_t counter1s = 0;
void AppTimer1ms() {
	uint32_t now = HAL_GetTick();
	AppProcess(now);
	RefreshActiveDevices(now);
	CheckMkuConfigMismatch();
	counter1s++;
	if(counter1s >= 1000) {
		counter1s = 0;
		AppSetStatus();
		status_sec_cnt++;

	}
}

void AppTimer10ms() {
	Button_Process();
	Beeper_Process();
	Led_Process();
}



void SetApp(uint32_t dst_adr, uint32_t src_adr, uint32_t sz) {

}

/* Установка системных времени и даты ППКУ по команде ServiceCmd_SetSystemTime.
 * Формат MsgData:
 *  [0] BCD HH
 *  [1] BCD MM
 *  [2] BCD SS
 *  [3] BCD YY (0..99)
 *  [4] BCD MM (1..12)
 *  [5] BCD DD (1..31)
 */
void RcvSetSystemTime(uint8_t *MsgData) {
	RTC_TimeTypeDef t = {0};
	t.Hours   = MsgData[0];
	t.Minutes = MsgData[1];
	t.Seconds = MsgData[2];
	t.SubSeconds = 0;
	RTC_DateTypeDef d;
	if (HAL_RTC_GetDate(&hrtc, &d, RTC_FORMAT_BCD) != HAL_OK) {
		return;
	}
	// Обновляем дату из команды, формат RTC: BCD YY/MM/DD
	d.Year  = MsgData[3];
	d.Month = MsgData[4];
	d.Date  = MsgData[5];
	if (HAL_RTC_SetTime(&hrtc, &t, RTC_FORMAT_BCD) != HAL_OK) {
		return;
	}
	HAL_RTC_SetDate(&hrtc, &d, RTC_FORMAT_BCD);
}
/*
uint32_t GetID() {
	uint32_t idPart1 = STM32_UUID[0];
	uint32_t idPart2 = STM32_UUID[1];
	uint32_t idPart3 = STM32_UUID[2];
	return (idPart1 ^ idPart2 ^ idPart3);
}
*/
void ResetMCU() {
	NVIC_SystemReset();
}




// посылки от устройств
void ListenerCommandCB(uint32_t MsgID, uint8_t *MsgData) {
	(void)MsgData;
	uint32_t now = HAL_GetTick();
	UpdateActiveDeviceList(MsgID, now);

	uint8_t Command = MsgData[0];
	if(Command >= ServiceCmd_SetStatusFire && Command <= ServiceCmd_StopExtinguishment) {
		if(Command == ServiceCmd_SetStatusFire) {
			HAL_RTC_GetDate(&hrtc, &fire.d, RTC_FORMAT_BIN);
			HAL_RTC_GetTime(&hrtc, &fire.t, RTC_FORMAT_BIN);
			fire.isfire = 1;
			fire.dev.ID = MsgID & 0xFFFFFFF;
			fire.zone = fire.dev.field.zone;
		}
	}
}










