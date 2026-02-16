#include <gui/model/Model.hpp>
#include <gui/model/ModelListener.hpp>

#ifndef SIMULATOR
#include "stm32h5xx_hal_rtc.h"
#endif

Model::Model() : modelListener(0), soundOn(true)
{
#ifndef SIMULATOR
    soundToggledCallback = 0;
#endif
}
unsigned char pos = 0;
void Model::tick()
{
#ifndef SIMULATOR
	if (modelListener)
	{
		RTC_TimeTypeDef sTime;
		RTC_DateTypeDef sDate;
		if (HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN) == HAL_OK &&
		    HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN) == HAL_OK)
		{
			modelListener->setDateTime(
				(uint8_t)sTime.Hours,
				(uint8_t)sTime.Minutes,
				(uint8_t)sTime.Seconds,
				(uint8_t)sDate.Date,
				(uint8_t)sDate.Month,
				(uint8_t)sDate.Year);
		}
	}

	if(setup_change) {
		setup_change = 0;

		modelListener->SetupMenuChangePos(pos++);
		if(pos >= 6)
			pos = 0;



	}
#endif
}

#ifndef SIMULATOR
void Model::notifySoundToggled(bool soundOn)
{
    if (soundToggledCallback)
        soundToggledCallback(soundOn);
}
#endif
