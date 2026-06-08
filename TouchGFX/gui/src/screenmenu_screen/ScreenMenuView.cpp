#include <gui/screenmenu_screen/ScreenMenuView.hpp>
#include <texts/TextKeysAndLanguages.hpp>
#include <touchgfx/TypedText.hpp>
#include <touchgfx/Unicode.hpp>
#include <cstdio>

ScreenMenuView::ScreenMenuView()
{

}

int16_t ScreenMenuView::getSelectedMenuIndex() const
{
    int16_t idx = (int16_t)scrollWheel1.getSelectedItem();
    if (idx < 0) {
        return 0;
    }
    if (idx > 4) {
        return 4;
    }
    return idx;
}

void ScreenMenuView::setMenuIndex(int16_t index)
{
    if (index < 0) {
        index = 0;
    }
    if (index > 4) {
        index = 4;
    }
    scrollWheel1.animateToItem(index, 10);
}

#ifndef SIMULATOR
void ScreenMenuView::initParamLineText()
{
    textAreatime_2.setVisible(false);
    paramLineText.setPosition(0, 49, 128, 15);
    paramLineText.setColor(textAreatime_2.getColor());
    paramLineText.setLinespacing(0);
    paramLineText.setTypedText(touchgfx::TypedText(T___SINGLEUSE_2J37));
    paramLineText.setWildcard(paramLineBuffer);
    paramLineBuffer[0] = 0;
    add(paramLineText);
}
#endif

void ScreenMenuView::updateParameterLine(int16_t selectedIndex, uint8_t fireMode, bool soundOn, bool beepBlocked)
{
#ifndef SIMULATOR
    char line[48] = {0};
    if (selectedIndex == 0) {
        const char* modeName = "Автоматический";
        if (fireMode == 1u) {
            modeName = "Автономный";
        } else if (fireMode == 2u) {
            modeName = "Ручной";
        }
        (void)std::snprintf(line, sizeof(line), "%s", modeName);
    } else if (selectedIndex == 1) {
        if (beepBlocked) {
            (void)std::snprintf(line, sizeof(line), "%s БЛОК.", soundOn ? "Вкл" : "Откл");
        } else {
            (void)std::snprintf(line, sizeof(line), "%s", soundOn ? "Вкл" : "Откл");
        }
    } else {
        line[0] = '\0';
    }
    Unicode::fromUTF8(reinterpret_cast<const uint8_t*>(line), paramLineBuffer, PARAM_LINE_SIZE);
    paramLineBuffer[PARAM_LINE_SIZE - 1] = 0;
    paramLineText.invalidate();
#else
    (void)selectedIndex;
    (void)fireMode;
    (void)soundOn;
    (void)beepBlocked;
#endif
}

void ScreenMenuView::setupScreen()
{
    ScreenMenuViewBase::setupScreen();
#ifndef SIMULATOR
    initParamLineText();
    for (int i = 0; i < scrollWheel1ListItems.getNumberOfDrawables(); i++)
    {
        scrollWheel1.itemChanged(i);
        scrollWheel1ListItems[i].updateText(i);
    }
    for (int i = 0; i < scrollWheel1_1ListItems.getNumberOfDrawables(); i++)
    {
        scrollWheel1_1.itemChanged(i);
    }
#endif
}

void ScreenMenuView::tearDownScreen()
{
    ScreenMenuViewBase::tearDownScreen();
}
#ifndef SIMULATOR
void ScreenMenuView::SetupMenuChangePos(uint8_t val) {
    uint8_t i = 0;
    if (val >= 3) {
        i = 1;
    }
    scrollWheel1_1.itemChanged(i);
}

void ScreenMenuView::scrollWheel1UpdateItem(mainmenu& item, int16_t itemIndex)
{
    if (itemIndex > 4) {
        itemIndex = 4;
    }
    item.updateText(itemIndex);
}
#endif
