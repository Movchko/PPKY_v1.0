#include <gui/screenmenu_jurnal_screen/ScreenMenu_jurnalView.hpp>
#include <gui/screenmenu_jurnal_screen/ScreenMenu_jurnalPresenter.hpp>
#include <gui/common/FrontendApplication.hpp>
#include <touchgfx/Application.hpp>
#include "button.h"

ScreenMenu_jurnalPresenter::ScreenMenu_jurnalPresenter(ScreenMenu_jurnalView& v)
    : view(v)
{
}

void ScreenMenu_jurnalPresenter::activate()
{
}

void ScreenMenu_jurnalPresenter::deactivate()
{
}

#ifndef SIMULATOR
void ScreenMenu_jurnalPresenter::handleButton(uint8_t but, uint8_t state)
{
    if (state != (uint8_t)ButtonStatePress) {
        return;
    }

    if (but == BUT_ESC) {
        FrontendApplication* app = static_cast<FrontendApplication*>(touchgfx::Application::getInstance());
        app->gotoScreenMenuScreenNoTransition();
    }
}
#endif
