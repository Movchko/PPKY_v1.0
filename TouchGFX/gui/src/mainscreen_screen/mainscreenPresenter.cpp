#include <gui/mainscreen_screen/mainscreenView.hpp>
#include <gui/mainscreen_screen/mainscreenPresenter.hpp>
#include <gui/common/FrontendApplication.hpp>
#include <touchgfx/Application.hpp>
#include "button.h"

mainscreenPresenter::mainscreenPresenter(mainscreenView& v)
    : view(v)
{

}

void mainscreenPresenter::activate()
{

}

void mainscreenPresenter::deactivate()
{

}

void mainscreenPresenter::setDateTime(uint8_t hour, uint8_t min, uint8_t sec, uint8_t day, uint8_t month, uint8_t year)
{
    view.setDateTime(hour, min, sec, day, month, year);
}

#ifndef SIMULATOR
void mainscreenPresenter::SetTime(uint32_t time) {
	view.SetTime(time);
}

void mainscreenPresenter::handleButton(uint8_t but, uint8_t state)
{
    if (but == BUT_ENTER && state == (uint8_t)ButtonStatePress)
    {
        FrontendApplication* app = static_cast<FrontendApplication*>(touchgfx::Application::getInstance());
        app->gotoScreenMenuScreenNoTransition();
    }
}

void mainscreenPresenter::onFireStatusChanged(bool active, uint8_t zone, uint8_t remaining_s, const char* zoneName)
{
	view.updateFireStatus(active, zone, remaining_s, zoneName);
}
#endif
