#include <gui/screenmenu_mcu_details_screen/ScreenMenu_MCU_DetailsView.hpp>
#include <texts/TextKeysAndLanguages.hpp>
#include <touchgfx/Unicode.hpp>

ScreenMenu_MCU_DetailsView::ScreenMenu_MCU_DetailsView()
{
}

#ifndef SIMULATOR
void ScreenMenu_MCU_DetailsView::initDetailText()
{
    textAreatime_2.setVisible(false);
    detailText.setPosition(textAreatime_2.getX(), textAreatime_2.getY(),
                           textAreatime_2.getWidth(), textAreatime_2.getHeight());
    detailText.setColor(textAreatime_2.getColor());
    detailText.setLinespacing(textAreatime_2.getLinespacing());
    detailText.setTypedText(touchgfx::TypedText(T___SINGLEUSE_2J37));
    detailText.setWildcard(detailTextBuffer);
    add(detailText);
}
#endif

void ScreenMenu_MCU_DetailsView::setDetailText(const char* text)
{
#ifndef SIMULATOR
    if (text == nullptr) {
        detailTextBuffer[0] = 0;
    } else {
        Unicode::fromUTF8(reinterpret_cast<const uint8_t*>(text), detailTextBuffer, DETAIL_TEXT_SIZE);
        detailTextBuffer[DETAIL_TEXT_SIZE - 1] = 0;
    }
    detailText.invalidate();
#else
    (void)text;
#endif
}

void ScreenMenu_MCU_DetailsView::setupScreen()
{
    ScreenMenu_MCU_DetailsViewBase::setupScreen();
#ifndef SIMULATOR
    initDetailText();
#endif
}

void ScreenMenu_MCU_DetailsView::tearDownScreen()
{
    ScreenMenu_MCU_DetailsViewBase::tearDownScreen();
}
