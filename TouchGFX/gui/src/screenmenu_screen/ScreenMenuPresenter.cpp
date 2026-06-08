#include <gui/screenmenu_screen/ScreenMenuView.hpp>
#include <gui/screenmenu_screen/ScreenMenuPresenter.hpp>
#include <gui/common/FrontendApplication.hpp>
#include <touchgfx/Application.hpp>
#include "button.h"
#include "device_config.h"
extern PPKYCfg PPKYConfig;
extern void SaveConfig(void);

ScreenMenuPresenter::ScreenMenuPresenter(ScreenMenuView& v)
    : view(v)
{
#ifndef SIMULATOR
    soundOn = true;
    currentIndex = 0;
#endif
}

void ScreenMenuPresenter::activate()
{
#ifndef SIMULATOR
    FrontendApplication* app = static_cast<FrontendApplication*>(touchgfx::Application::getInstance());
    soundOn = (PPKYConfig.beep != 0u);
    app->getModel().setSoundOn(soundOn);
    currentIndex = view.getSelectedMenuIndex();
    refreshLine();
#endif
}

void ScreenMenuPresenter::deactivate()
{
}

#ifndef SIMULATOR
void ScreenMenuPresenter::refreshLine()
{
    view.updateParameterLine(currentIndex, PPKYConfig.fire_mode, soundOn, PPKYConfig.beep_block != 0u);
}

void ScreenMenuPresenter::SetupMenuChangePos(unsigned char val) {
    view.SetupMenuChangePos(val);
}

void ScreenMenuPresenter::handleButton(uint8_t but, uint8_t state)
{
    if (state != (uint8_t)ButtonStatePress) {
        return;
    }

    FrontendApplication* app = static_cast<FrontendApplication*>(touchgfx::Application::getInstance());

    if (but == BUT_ESC) {
        app->gotomainscreenScreenNoTransition();
        return;
    }

    if (but == BUT_UP) {
        currentIndex = (currentIndex - 1 + MENU_ITEMS) % MENU_ITEMS;
        view.setMenuIndex(currentIndex);
        refreshLine();
        return;
    }

    if (but == BUT_DOWN) {
        currentIndex = (currentIndex + 1) % MENU_ITEMS;
        view.setMenuIndex(currentIndex);
        refreshLine();
        return;
    }

    if (but == BUT_ENTER) {
        if (currentIndex == 0) {
            uint8_t mode = (uint8_t)((PPKYConfig.fire_mode + 1u) % 3u);
            PPKYConfig.fire_mode = mode;
            SaveConfig();
            refreshLine();
            return;
        }
        if (currentIndex == 1) {
            if (PPKYConfig.beep_block != 0u) {
                refreshLine();
                return;
            }
            soundOn = !soundOn;
            PPKYConfig.beep = soundOn ? 1u : 0u;
            SaveConfig();
            app->getModel().setSoundOn(soundOn);
            app->getModel().notifySoundToggled(soundOn);
            refreshLine();
            return;
        }
        if (currentIndex == 2) {
            app->gotoScreenMenuConnectionScreenNoTransition();
            return;
        }
        if (currentIndex == 3) {
            app->gotoScreenMenuJurnalScreenNoTransition();
            return;
        }
        if (currentIndex == 4) {
            app->gotoScreenDevicesScreenNoTransition();
            return;
        }
    }
}
#endif
