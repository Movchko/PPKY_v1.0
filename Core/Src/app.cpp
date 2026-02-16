#include "power_control.h"
#include "app.hpp"
#include "button.h"
#include "beeper.h"
#include "device_config.h"
#include "led.h"
#include "backend.h"
#include "gui/common/FrontendHeap.hpp"

struct PPKYCfg PPKYConfig;       // локальная (рабочая) конфигурация
static struct PPKYCfg SavedPPKYConfig; // копия сохранённой конфигурации из Flash



PControl *Power[2];


void USBSendData(uint8_t *Buf) {};
void CommandCB(uint8_t Dev, uint8_t Command, uint8_t *Parameters) {};

GPIO_TypeDef   *POWER_ST_PORT[2] = {ST1_MK_GPIO_Port, ST2_MK_GPIO_Port};
uint16_t  		POWER_ST_PIN[2] = {ST1_MK_Pin, ST2_MK_Pin};
GPIO_TypeDef   *POWER_OUT_PORT[2] = {KEY_1_GPIO_Port, KEY_2_GPIO_Port};
uint16_t  		POWER_OUT_PIN[2] = {KEY_1_Pin, KEY_2_Pin};

bool isAppInit = 0;

extern SPIF_HandleTypeDef hFlash;

uint8_t PControlGetSTCB(uint8_t ch) {
	uint8_t st = 0;
	st = HAL_GPIO_ReadPin(POWER_ST_PORT[ch], POWER_ST_PIN[ch]);
	return st;
}

uint32_t PControlGetADCCB(uint8_t ch) {
	uint32_t code = 0;

	return code;
}

void PControlSetOutCB(uint8_t ch, uint8_t out) {
	HAL_GPIO_WritePin(POWER_OUT_PORT[ch], POWER_OUT_PIN[ch], (GPIO_PinState)out);
}

void AppInit() {

	// Чтение сохранённой конфигурации из Flash (область конфигурации)
	uint32_t cfg_addr = SPIF_SectorToAddress(FLASH_CFG_START_SECTOR);
	PPKYConfigHeader hdr;
	SPIF_ReadAddress(&hFlash, cfg_addr, (uint8_t *)&hdr, sizeof(hdr));

	bool header_ok = (hdr.magic == PPKY_CFG_HEADER_MAGIC) &&
			         (hdr.size  == sizeof(PPKYConfig));

	if (header_ok) {
		// Заголовок валиден — читаем полезную часть
		SPIF_ReadAddress(&hFlash,
				         cfg_addr + sizeof(PPKYConfigHeader),
						 (uint8_t *)&SavedPPKYConfig,
						 sizeof(SavedPPKYConfig));
		PPKYConfig = SavedPPKYConfig;
	} else {
		// Заголовок мусор: считаем, что конфигурации нет
		// Сбрасываем на значения по умолчанию и сохраняем в область конфигурации
		DefaultConfig();
		SaveConfig();
	}

	// Передаём указатели в backend (для сервисных команд работы с конфигурацией)
	SetConfigPtr((uint8_t *)&SavedPPKYConfig, (uint8_t *)&PPKYConfig);

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
}

volatile uint8_t set[2] = {1, 1};
volatile uint8_t st[2];
void AppProcess() {
	if(isAppInit == false)
		return;
	for(uint8_t i = 0; i < 2; i++) {
		Power[i]->PControlSetOut(i, set[i]);
		st[i] = Power[i]->PControlGetST(i);
	}

}
uint32_t counter1s = 0;
void AppTimer1ms() {
	AppProcess();
	counter1s++;
	if(counter1s >= 1000) {
		counter1s = 0;
		uint32_t tick = HAL_GetTick();
		uint8_t data[8] = {128, uint8_t(tick>>24 & 0xff), uint8_t(tick>>16 & 0xff), uint8_t(tick>>8 & 0xff), uint8_t(tick>>0 & 0xff), 0, 0, 0};
		can_ext_id_t can_id;
		can_id.field.dir = 1;
		can_id.field.h_adr = 0;
		can_id.field.l_adr = 0;
		can_id.field.zone = 0;
		can_id.field.d_type = 0;
		SendMessageFull(can_id, data, 1);
	}
}

void AppTimer10ms() {
	Button_Process();
	Beeper_Process();
	Led_Process();
}

void FlashWriteData(uint8_t *ConfigPtr, uint16_t ConfigSize) {
	// Конфигурация хранится начиная с FLASH_CFG_START_SECTOR, занимает FLASH_CFG_NUM_SECTOR секторов.
	// В начале области лежит заголовок PPKYConfigHeader, затем байты структуры PPKYCfg.
	uint32_t cfg_addr = SPIF_SectorToAddress(FLASH_CFG_START_SECTOR);

	for (uint32_t s = 0; s < FLASH_CFG_NUM_SECTOR; s++) {
		SPIF_EraseSector(&hFlash, FLASH_CFG_START_SECTOR + s);
	}

	PPKYConfigHeader hdr;
	hdr.magic   = PPKY_CFG_HEADER_MAGIC;
	hdr.version = 1;
	hdr.size    = ConfigSize;

	// Сначала пишем заголовок
	SPIF_WriteAddress(&hFlash, cfg_addr, (uint8_t *)&hdr, sizeof(hdr));
	// Затем полезные данные конфигурации сразу после заголовка
	SPIF_WriteAddress(&hFlash, cfg_addr + sizeof(PPKYConfigHeader), ConfigPtr, ConfigSize);
}

void SetApp(uint32_t dst_adr, uint32_t src_adr, uint32_t sz) {

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

void DefaultConfig() {
	// Установка конфигурации по умолчанию в локальный буфер (PPKYConfig)
	memset(&PPKYConfig, 0, sizeof(PPKYConfig));

	// Заполняем UniqId из уникального идентификатора STM
	uint32_t uid0 = HAL_GetUIDw0();
	uint32_t uid1 = HAL_GetUIDw1();
	uint32_t uid2 = HAL_GetUIDw2();

	PPKYConfig.UId.UId0 = uid0;
	PPKYConfig.UId.UId1 = uid1;
	PPKYConfig.UId.UId2 = uid2;
	PPKYConfig.UId.UId3 = HAL_GetDEVID();
	PPKYConfig.UId.UId4 = 1;

	PPKYConfig.UId.devId.zone  = 0;
	PPKYConfig.UId.devId.l_adr = 0;

	uint8_t hadr = (uint8_t)(uid0 & 0xFF);
	if (hadr == 0) {
		hadr = (uint8_t)(uid1 & 0xFF);
		if (hadr == 0) {
			hadr = 1; // на всякий случай, чтобы не был 0
		}
	}
	PPKYConfig.UId.devId.h_adr = hadr;
	PPKYConfig.UId.devId.d_type = DEVICE_PPKY_TYPE;

	// Примеры значений по умолчанию
	PPKYConfig.beep = 1; // звук включен

	// Имена зон очищаем (пустые строки)
	for (uint16_t i = 0; i < ZONE_NUMBER; i++) {
		memset(PPKYConfig.zone_name[i], 0, ZONE_NAME_SIZE);
	}

	// reserv оставляем нулевым
}

// посылки от устройств
void ListenerCommandCB(uint32_t MsgID, uint8_t *MsgData) {
	//TODO
}

// Размер конфигурации (в байтах)
uint16_t GetConfigSize() { // get config size in bytes
	return (uint16_t)sizeof(PPKYConfig);
}

// Чтение 4-байтового слова из локальной конфигурации (big-endian)
uint32_t GetConfigWord(uint16_t num) { // get 4 bytes
	uint32_t byte_index = (uint32_t)num * 4U;
	uint16_t cfg_size = GetConfigSize();

	if (byte_index + 4U > cfg_size) {
		// За пределами диапазона – возвращаем 0
		return 0;
	}

	uint8_t *p = (uint8_t *)&PPKYConfig;
	uint32_t word = 0;
	word |= ((uint32_t)p[byte_index + 0] << 24);
	word |= ((uint32_t)p[byte_index + 1] << 16);
	word |= ((uint32_t)p[byte_index + 2] << 8);
	word |= ((uint32_t)p[byte_index + 3] << 0);

	return word;
}

// Запись 4-байтового слова в локальную конфигурацию (big-endian)
void SetConfigWord(uint16_t num, uint32_t word) { // set 4 bytes
	uint32_t byte_index = (uint32_t)num * 4U;
	uint16_t cfg_size = GetConfigSize();

	if (byte_index + 4U > cfg_size) {
		// За пределами диапазона – игнорируем
		return;
	}

	uint8_t *p = (uint8_t *)&PPKYConfig;
	p[byte_index + 0] = (uint8_t)((word >> 24) & 0xFF);
	p[byte_index + 1] = (uint8_t)((word >> 16) & 0xFF);
	p[byte_index + 2] = (uint8_t)((word >> 8)  & 0xFF);
	p[byte_index + 3] = (uint8_t)((word >> 0)  & 0xFF);
}

// Сохранение локальной конфигурации в Flash и обновление копии SavedPPKYConfig
void SaveConfig() {
	uint16_t size = GetConfigSize();
	FlashWriteData((uint8_t *)&PPKYConfig, size);

	// Читаем обратно из Flash в SavedPPKYConfig — проверяем, что запись прошла
	uint32_t cfg_addr = SPIF_SectorToAddress(FLASH_CFG_START_SECTOR);
	PPKYConfigHeader hdr;
	SPIF_ReadAddress(&hFlash, cfg_addr, (uint8_t *)&hdr, sizeof(hdr));

	if ((hdr.magic == PPKY_CFG_HEADER_MAGIC) && (hdr.size == size)) {
		SPIF_ReadAddress(&hFlash,
				         cfg_addr + sizeof(PPKYConfigHeader),
						 (uint8_t *)&SavedPPKYConfig,
						 size);
	} else {
		// Что-то пошло не так, оставляем SavedPPKYConfig равным локальной конфигурации
		SavedPPKYConfig = PPKYConfig;
	}
}


