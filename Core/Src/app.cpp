#include "power_control.h"
#include "app.hpp"
#include "button.h"
#include "beeper.h"

PControl *Power[2];


void USBSendData(uint8_t *Buf) {};
void CommandCB(uint8_t Dev, uint8_t Command, uint8_t *Parameters) {};

GPIO_TypeDef   *POWER_ST_PORT[2] = {ST1_MK_GPIO_Port, ST2_MK_GPIO_Port};
uint16_t  		POWER_ST_PIN[2] = {ST1_MK_Pin, ST2_MK_Pin};
GPIO_TypeDef   *POWER_OUT_PORT[2] = {KEY_1_GPIO_Port, KEY_2_GPIO_Port};
uint16_t  		POWER_OUT_PIN[2] = {KEY_1_Pin, KEY_2_Pin};

bool isAppInit = 0;

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

	Button_Init();
	Beeper_Init();

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
}

