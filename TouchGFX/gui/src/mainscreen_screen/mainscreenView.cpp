#include <gui/mainscreen_screen/mainscreenView.hpp>
#include <cstdio>
#include <cstring>

#ifndef SIMULATOR
#include "main.h"
#include "device_config.h"

namespace {

constexpr uint32_t FIRE_NAME_HOLD_MS = 3000u;

enum FireNamePhase : uint8_t {
	PH_IDLE = 0,
	PH_WAIT_LONG_SCROLL,
	PH_HOLD_3S
};

enum UiBannerMode : uint8_t {
	BANNER_NONE = 0,
	BANNER_FIRE,
	BANNER_WARNING
};

mainscreenView* g_fire_main_view = nullptr;

uint8_t s_fn_n = 0;
char s_fn_names[16][ZONE_NAME_SIZE + 1];
uint8_t s_fn_cur = 0;
FireNamePhase s_fn_ph = PH_IDLE;
uint32_t s_fn_hold_from = 0;
UiBannerMode s_banner_mode = BANNER_NONE;

uint8_t s_wn_n = 0;
char s_wn_titles[16][16];
char s_wn_details[16][ZONE_NAME_SIZE + 1];
uint8_t s_wn_cur = 0;
FireNamePhase s_wn_ph = PH_IDLE;
uint32_t s_wn_hold_from = 0;

static void fire_copy_list(uint8_t n, char (*src)[ZONE_NAME_SIZE + 1])
{
	s_fn_n = (n > 16u) ? 16u : n;
	for (uint8_t i = 0u; i < s_fn_n; i++) {
		std::strncpy(s_fn_names[i], src[i], ZONE_NAME_SIZE);
		s_fn_names[i][ZONE_NAME_SIZE] = '\0';
	}
}

static bool fire_list_equals(uint8_t n, char (*src)[ZONE_NAME_SIZE + 1])
{
	if (n != s_fn_n) {
		return false;
	}
	for (uint8_t i = 0u; i < n; i++) {
		if (std::strncmp(s_fn_names[i], src[i], ZONE_NAME_SIZE + 1) != 0) {
			return false;
		}
	}
	return true;
}

static void fire_marquee_done_thunk(CustomContainerSollText*)
{
	if (g_fire_main_view != nullptr) {
		if (s_banner_mode == BANNER_FIRE) {
			g_fire_main_view->fireOnMarqueeOnePassDone();
		} else if (s_banner_mode == BANNER_WARNING) {
			g_fire_main_view->warningOnMarqueeOnePassDone();
		}
	}
}

} // namespace
#endif

mainscreenView::mainscreenView()
{

}

void mainscreenView::setupScreen()
{
    mainscreenViewBase::setupScreen();

    /* По умолчанию пустая бегущая строка, будет задаваться из приложения (пожар, ошибки и т.п.) */
    CustomContainerSrollText.setText("");
#ifndef SIMULATOR
    g_fire_main_view = this;
    CustomContainerSrollText.setFinishedCallback(fire_marquee_done_thunk);
#endif
}

void mainscreenView::tearDownScreen()
{
#ifndef SIMULATOR
    if (g_fire_main_view == this) {
        g_fire_main_view = nullptr;
    }
    CustomContainerSrollText.setFinishedCallback(nullptr);
#endif
    mainscreenViewBase::tearDownScreen();
}

void mainscreenView::setDateTime(uint8_t hour, uint8_t min, uint8_t sec, uint8_t day, uint8_t month, uint8_t year)
{
    customContainerScrollTime1.setTime(hour, min, sec, day, month, year);
}

#ifndef SIMULATOR
void mainscreenView::fireOnMarqueeOnePassDone()
{
	if (s_fn_ph == PH_WAIT_LONG_SCROLL) {
		s_fn_ph = PH_HOLD_3S;
		s_fn_hold_from = HAL_GetTick();
	}
}

void mainscreenView::fireShowCurrentZone()
{
	if (s_fn_n == 0u) {
		return;
	}
	CustomContainerSrollText.setText(s_fn_names[s_fn_cur]);
	s_banner_mode = BANNER_FIRE;
	if (CustomContainerSrollText.isMarqueeFitting()) {
		s_fn_ph = PH_HOLD_3S;
		s_fn_hold_from = HAL_GetTick();
	} else {
		s_fn_ph = PH_WAIT_LONG_SCROLL;
	}
}

void mainscreenView::warningOnMarqueeOnePassDone()
{
	if (s_wn_ph == PH_WAIT_LONG_SCROLL) {
		s_wn_ph = PH_HOLD_3S;
		s_wn_hold_from = HAL_GetTick();
	}
}

void mainscreenView::warningShowCurrent()
{
	if (s_wn_n == 0u) {
		return;
	}
	s_banner_mode = BANNER_WARNING;

	for (uint16_t i = 0; i < TEXTAREA1_SIZE; i++) {
		textArea1Buffer[i] = 0;
	}
	Unicode::fromUTF8(reinterpret_cast<const uint8_t*>(s_wn_titles[s_wn_cur]), textArea1Buffer, TEXTAREA1_SIZE);
	textArea1Buffer[TEXTAREA1_SIZE - 1] = 0;
	textArea1.setWildcard(textArea1Buffer);
	textArea1.invalidate();

	CustomContainerSrollText.setText(s_wn_details[s_wn_cur]);
	if (CustomContainerSrollText.isMarqueeFitting()) {
		s_wn_ph = PH_HOLD_3S;
		s_wn_hold_from = HAL_GetTick();
	} else {
		s_wn_ph = PH_WAIT_LONG_SCROLL;
	}
}

void mainscreenView::SetTime(uint32_t time) {

};

void mainscreenView::updateFireStatus(bool active, uint8_t zone, uint8_t remaining_s, uint8_t nZoneNames,
				      char (*zoneNames)[ZONE_NAME_SIZE + 1])
{
	(void)zone;
	uint32_t now = HAL_GetTick();
	fireUiActive = active;

	static uint8_t lastActive = 0xFFu;
	static uint8_t lastRemaining = 0xFFu;
	const bool timerDirty = ((uint8_t)active != lastActive || remaining_s != lastRemaining);

	if (!active) {
		lastActive = (uint8_t)active;
		lastRemaining = remaining_s;
		s_fn_ph = PH_IDLE;
		s_fn_n = 0u;
		/* Если сейчас отображается предупреждение, не затираем поля:
		 * warning-логика использует те же widgets и сама их контролирует. */
		if (s_banner_mode != BANNER_WARNING) {
			for (uint16_t i = 0; i < TEXTAREA1_SIZE; i++) {
				textArea1Buffer[i] = 0;
			}
			Unicode::snprintf(textArea1Buffer, TEXTAREA1_SIZE, "%s", "");
			textArea1.setWildcard(textArea1Buffer);
			textArea1.invalidate();
			CustomContainerSrollText.setText("");
			s_banner_mode = BANNER_NONE;
		}
		return;
	}

	if (nZoneNames > 0u && !fire_list_equals(nZoneNames, zoneNames)) {
		fire_copy_list(nZoneNames, zoneNames);
		s_fn_cur = 0u;
		s_fn_ph = PH_IDLE;
		s_fn_hold_from = 0u;
		fireShowCurrentZone();
	}

	if (timerDirty) {
		lastActive = (uint8_t)active;
		lastRemaining = remaining_s;
		for (uint16_t i = 0; i < TEXTAREA1_SIZE; i++) {
			textArea1Buffer[i] = 0;
		}
		char buf[16];
		if (remaining_s > 0) {
			snprintf(buf, sizeof(buf), "%u", (unsigned)remaining_s);
		} else {
			buf[0] = '\0';
		}
		Unicode::fromUTF8(reinterpret_cast<const uint8_t*>(buf), textArea1Buffer, TEXTAREA1_SIZE);
		textArea1Buffer[TEXTAREA1_SIZE - 1] = 0;
		textArea1.setWildcard(textArea1Buffer);
		textArea1.invalidate();
	}

	if (nZoneNames == 0u) {
		/* Не затираем бегущую строку при active: Model может ещё не получить список зон,
		 * а тики TouchGFX с n==0 иначе держат пустоту до смены секунды таймера. */
		if (!active) {
			CustomContainerSrollText.setText("");
		}
		return;
	}

	if (s_fn_ph == PH_HOLD_3S && s_fn_hold_from != 0u &&
	    (now - s_fn_hold_from) >= FIRE_NAME_HOLD_MS) {
		s_fn_cur = (uint8_t)((s_fn_cur + 1u) % s_fn_n);
		s_fn_ph = PH_IDLE;
		s_fn_hold_from = 0u;
	}

	if (s_fn_ph == PH_IDLE) {
		fireShowCurrentZone();
	}
}

void mainscreenView::updateWarningStatus(bool active, uint8_t nItems, char (*bigTitles)[16],
					 char (*details)[ZONE_NAME_SIZE + 1])
{
	if (fireUiActive) {
		return;
	}
	uint32_t now = HAL_GetTick();

	if (!active || nItems == 0u) {
		s_wn_n = 0u;
		s_wn_ph = PH_IDLE;
		s_wn_hold_from = 0u;
		if (s_banner_mode == BANNER_WARNING) {
			for (uint16_t i = 0; i < TEXTAREA1_SIZE; i++) {
				textArea1Buffer[i] = 0;
			}
			Unicode::snprintf(textArea1Buffer, TEXTAREA1_SIZE, "%s", "");
			textArea1.setWildcard(textArea1Buffer);
			textArea1.invalidate();
			CustomContainerSrollText.setText("");
			s_banner_mode = BANNER_NONE;
		}
		return;
	}

	if (nItems > 16u) {
		nItems = 16u;
	}

	bool changed = (nItems != s_wn_n);
	if (!changed) {
		for (uint8_t i = 0u; i < nItems && !changed; i++) {
			if (std::strncmp(s_wn_titles[i], bigTitles[i], 16) != 0 ||
			    std::strncmp(s_wn_details[i], details[i], ZONE_NAME_SIZE + 1) != 0) {
				changed = true;
			}
		}
	}

	if (changed) {
		/* Пытаемся сохранить текущую позицию ротации, если текущая строка
		 * присутствует и в новом списке (это устраняет визуальное "смаргивание"). */
		uint8_t keep_idx = 0u;
		uint8_t keep_found = 0u;
		if (s_wn_n > 0u && s_wn_cur < s_wn_n) {
			for (uint8_t i = 0u; i < nItems; i++) {
				if (std::strncmp(s_wn_titles[s_wn_cur], bigTitles[i], 16) == 0 &&
				    std::strncmp(s_wn_details[s_wn_cur], details[i], ZONE_NAME_SIZE + 1) == 0) {
					keep_idx = i;
					keep_found = 1u;
					break;
				}
			}
		}
		s_wn_n = nItems;
		for (uint8_t i = 0u; i < s_wn_n; i++) {
			std::strncpy(s_wn_titles[i], bigTitles[i], 15u);
			s_wn_titles[i][15] = '\0';
			std::strncpy(s_wn_details[i], details[i], ZONE_NAME_SIZE);
			s_wn_details[i][ZONE_NAME_SIZE] = '\0';
		}
		s_wn_cur = keep_found ? keep_idx : 0u;
		s_wn_ph = PH_IDLE;
		s_wn_hold_from = 0u;
		warningShowCurrent();
	}

	if (s_wn_n == 0u) {
		return;
	}

	if (s_wn_ph == PH_HOLD_3S && s_wn_hold_from != 0u &&
	    (now - s_wn_hold_from) >= FIRE_NAME_HOLD_MS) {
		s_wn_cur = (uint8_t)((s_wn_cur + 1u) % s_wn_n);
		s_wn_ph = PH_IDLE;
		s_wn_hold_from = 0u;
	}

	if (s_wn_ph == PH_IDLE) {
		warningShowCurrent();
	}
}
#endif
