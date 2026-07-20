#include <gui/screenblockzone_screen/ScreenBlockZoneView.hpp>
#include <touchgfx/Unicode.hpp>
#include <cstring>

#ifndef SIMULATOR
#include "device_config.h"
#include "config_zone_block.h"

extern PPKYCfg PPKYConfig;
#endif

ScreenBlockZoneView::ScreenBlockZoneView()
#ifndef SIMULATOR
    : activeZoneCount_(0u),
      selectedPos_(0u)
#endif
{
}

void ScreenBlockZoneView::setupScreen()
{
    ScreenBlockZoneViewBase::setupScreen();
#ifndef SIMULATOR
    activeZoneCount_ = 0u;
    selectedPos_ = 0u;
    refreshZoneUi();
#endif
}

void ScreenBlockZoneView::tearDownScreen()
{
    ScreenBlockZoneViewBase::tearDownScreen();
}

#ifndef SIMULATOR
static void trimZoneName(char *dst, size_t dst_sz, const int8_t *src)
{
    if (dst == nullptr || dst_sz == 0u || src == nullptr) {
        return;
    }
    size_t n = 0u;
    while (n + 1u < dst_sz && src[n] != 0) {
        dst[n] = (char)src[n];
        n++;
    }
    dst[n] = '\0';
    while (n > 0u && (dst[n - 1u] == ' ')) {
        dst[n - 1u] = '\0';
        n--;
    }
}

void ScreenBlockZoneView::refreshZoneUi()
{
    activeZoneCount_ = 0u;
    for (uint8_t zi = 0u; zi < ZONE_NUMBER; zi++) {
        if (PPKYConfig.zone_name[zi][0] == 0) {
            continue;
        }
        if (activeZoneCount_ < MAX_ACTIVE_ZONES) {
            activeZoneIdx_[activeZoneCount_++] = zi;
        }
    }

    if (activeZoneCount_ == 0u) {
        CustomContainerSrollText.setText("");
        for (uint16_t i = 0u; i < TEXTAREATIME_ON_OFF_SIZE; i++) {
            textAreatime_on_offBuffer[i] = 0u;
        }
        Unicode::fromUTF8(reinterpret_cast<const uint8_t *>(""),
                          textAreatime_on_offBuffer,
                          TEXTAREATIME_ON_OFF_SIZE);
        textAreatime_on_offBuffer[TEXTAREATIME_ON_OFF_SIZE - 1u] = 0u;
        textAreatime_on_off.invalidate();
        return;
    }

    if (selectedPos_ >= activeZoneCount_) {
        selectedPos_ = 0u;
    }

    const uint8_t zone_idx = activeZoneIdx_[selectedPos_];
    char zone_name[ZONE_NAME_SIZE + 1];
    trimZoneName(zone_name, sizeof(zone_name), PPKYConfig.zone_name[zone_idx]);
    CustomContainerSrollText.setText(zone_name);

    const char *status = PPKY_ZoneBlockGet(PPKYConfig.zone_block, zone_idx) ? "ЗАБЛОК." : "НЕ ЗАБЛОК.";
    for (uint16_t i = 0u; i < TEXTAREATIME_ON_OFF_SIZE; i++) {
        textAreatime_on_offBuffer[i] = 0u;
    }
    Unicode::fromUTF8(reinterpret_cast<const uint8_t *>(status),
                      textAreatime_on_offBuffer,
                      TEXTAREATIME_ON_OFF_SIZE);
    textAreatime_on_offBuffer[TEXTAREATIME_ON_OFF_SIZE - 1u] = 0u;
    textAreatime_on_off.invalidate();
}

void ScreenBlockZoneView::nextActiveZone()
{
    if (activeZoneCount_ == 0u) {
        return;
    }
    selectedPos_ = (uint8_t)((selectedPos_ + 1u) % activeZoneCount_);
    refreshZoneUi();
}

void ScreenBlockZoneView::prevActiveZone()
{
    if (activeZoneCount_ == 0u) {
        return;
    }
    selectedPos_ = (uint8_t)((selectedPos_ == 0u) ? (activeZoneCount_ - 1u) : (selectedPos_ - 1u));
    refreshZoneUi();
}

void ScreenBlockZoneView::toggleSelectedZoneBlock()
{
    if (activeZoneCount_ == 0u) {
        return;
    }
    const uint8_t zone_idx = activeZoneIdx_[selectedPos_];
    const uint8_t blocked = PPKY_ZoneBlockGet(PPKYConfig.zone_block, zone_idx);
    PPKY_ZoneBlockSet(PPKYConfig.zone_block, zone_idx, blocked ? 0u : 1u);
    refreshZoneUi();
}

uint8_t ScreenBlockZoneView::hasActiveZones() const
{
    return (activeZoneCount_ > 0u) ? 1u : 0u;
}
#endif
