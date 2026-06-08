#include <gui/screenmenu_mcu_details_screen/ScreenMenu_MCU_DetailsView.hpp>
#include <gui/screenmenu_mcu_details_screen/ScreenMenu_MCU_DetailsPresenter.hpp>
#include <gui/common/FrontendApplication.hpp>
#include <touchgfx/Application.hpp>
#include "button.h"
#include "device_config.h"
#include "menu_ui.h"
#include <cstdio>

extern PPKYCfg PPKYConfig;

ScreenMenu_MCU_DetailsPresenter::ScreenMenu_MCU_DetailsPresenter(ScreenMenu_MCU_DetailsView& v)
    : view(v)
{
}

void ScreenMenu_MCU_DetailsPresenter::activate()
{
#ifndef SIMULATOR
    uint8_t slot = MenuUi_GetMcuDetailSlot();
    if (slot >= 32u) {
        view.setDetailText("Нет данных");
        return;
    }

    const MKUCfg* mku = &PPKYConfig.CfgDevices[slot];
    const Device* dev = &mku->UId.devId;

    char line[160] = {0};
    (void)std::snprintf(line, sizeof(line),
        "h=%u z=%u t=%u\n%08lX:%08lX:%08lX",
        (unsigned)dev->h_adr,
        (unsigned)dev->zone,
        (unsigned)dev->d_type,
        (unsigned long)mku->UId.UId0,
        (unsigned long)mku->UId.UId1,
        (unsigned long)mku->UId.UId2);
    view.setDetailText(line);
#endif
}

void ScreenMenu_MCU_DetailsPresenter::deactivate()
{
}

#ifndef SIMULATOR
void ScreenMenu_MCU_DetailsPresenter::handleButton(uint8_t but, uint8_t state)
{
    if (state != (uint8_t)ButtonStatePress) {
        return;
    }

    if (but == BUT_ESC) {
        FrontendApplication* app = static_cast<FrontendApplication*>(touchgfx::Application::getInstance());
        app->gotoScreenDevicesScreenNoTransition();
    }
}
#endif
