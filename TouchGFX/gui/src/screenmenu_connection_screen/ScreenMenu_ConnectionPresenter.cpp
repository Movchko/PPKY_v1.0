#include <gui/screenmenu_connection_screen/ScreenMenu_ConnectionView.hpp>
#include <gui/screenmenu_connection_screen/ScreenMenu_ConnectionPresenter.hpp>
#include <gui/common/FrontendApplication.hpp>
#include <touchgfx/Application.hpp>
#include "button.h"
#include "device_config.h"
#include "esp_manager.h"

extern PPKYCfg PPKYConfig;
extern void SaveConfig(void);

ScreenMenu_ConnectionPresenter::ScreenMenu_ConnectionPresenter(ScreenMenu_ConnectionView& v)
    : view(v)
{
#ifndef SIMULATOR
    currentIndex = 0;
#endif
}

void ScreenMenu_ConnectionPresenter::activate()
{
#ifndef SIMULATOR
    currentIndex = view.getSelectedIndex();
    if (PPKYConfig.wifi_block != 0u) {
        currentIndex = 1;
        view.setSelectedIndex(currentIndex);
    }
    refreshLine();
#endif
}

void ScreenMenu_ConnectionPresenter::deactivate()
{
}

#ifndef SIMULATOR
void ScreenMenu_ConnectionPresenter::refreshLine()
{
    view.updateStatusLine(currentIndex, PPKYConfig.wifi_block != 0u,
                          EspManager_IsUserWifiOn() != 0u, PPKYConfig.rs485_on != 0u);
}

void ScreenMenu_ConnectionPresenter::handleButton(uint8_t but, uint8_t state)
{
    if (state != (uint8_t)ButtonStatePress) {
        return;
    }

    FrontendApplication* app = static_cast<FrontendApplication*>(touchgfx::Application::getInstance());

    if (but == BUT_ESC) {
        app->gotoScreenMenuScreenNoTransition();
        return;
    }

    if (but == BUT_UP) {
        if (PPKYConfig.wifi_block != 0u) {
            currentIndex = 1;
        } else {
            currentIndex = (currentIndex - 1 + 2) % 2;
        }
        view.setSelectedIndex(currentIndex);
        refreshLine();
        return;
    }

    if (but == BUT_DOWN) {
        if (PPKYConfig.wifi_block != 0u) {
            currentIndex = 1;
        } else {
            currentIndex = (currentIndex + 1) % 2;
        }
        view.setSelectedIndex(currentIndex);
        refreshLine();
        return;
    }

    if (but == BUT_ENTER) {
        if (currentIndex == 0) {
            if (PPKYConfig.wifi_block != 0u) {
                refreshLine();
                return;
            }
            if (EspManager_IsUserWifiOn() != 0u) {
                EspManager_RequestWifiDisable();
            } else {
                EspManager_RequestWifiEnable();
            }
        } else {
            PPKYConfig.rs485_on = (PPKYConfig.rs485_on == 0u) ? 1u : 0u;
            SaveConfig();
        }
        refreshLine();
    }
}
#endif
