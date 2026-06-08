#ifndef SCREENMENU_MCU_DETAILSVIEW_HPP
#define SCREENMENU_MCU_DETAILSVIEW_HPP

#include <gui_generated/screenmenu_mcu_details_screen/ScreenMenu_MCU_DetailsViewBase.hpp>
#include <gui/screenmenu_mcu_details_screen/ScreenMenu_MCU_DetailsPresenter.hpp>
#include <touchgfx/widgets/TextAreaWithWildcard.hpp>

class ScreenMenu_MCU_DetailsView : public ScreenMenu_MCU_DetailsViewBase
{
public:
    ScreenMenu_MCU_DetailsView();
    virtual ~ScreenMenu_MCU_DetailsView() {}
    virtual void setupScreen();
    virtual void tearDownScreen();

    void setDetailText(const char* text);
protected:
#ifndef SIMULATOR
    static const uint16_t DETAIL_TEXT_SIZE = 128;
    touchgfx::Unicode::UnicodeChar detailTextBuffer[DETAIL_TEXT_SIZE];
    touchgfx::TextAreaWithOneWildcard detailText;
    void initDetailText();
#endif
};

#endif // SCREENMENU_MCU_DETAILSVIEW_HPP
