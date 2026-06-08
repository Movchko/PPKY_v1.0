#ifndef SCREENMENUVIEW_HPP
#define SCREENMENUVIEW_HPP

#include <gui_generated/screenmenu_screen/ScreenMenuViewBase.hpp>
#include <gui/screenmenu_screen/ScreenMenuPresenter.hpp>
#include <touchgfx/widgets/TextAreaWithWildcard.hpp>

class ScreenMenuView : public ScreenMenuViewBase
{
public:
    ScreenMenuView();
    virtual ~ScreenMenuView() {}
    virtual void setupScreen();
    virtual void tearDownScreen();

    int16_t getSelectedMenuIndex() const;
    void setMenuIndex(int16_t index);
    void updateParameterLine(int16_t selectedIndex, uint8_t fireMode, bool soundOn, bool beepBlocked);

#ifndef SIMULATOR
    virtual void SetupMenuChangePos(uint8_t val);
    virtual void scrollWheel1UpdateItem(mainmenu& item, int16_t itemIndex);
#endif
protected:
#ifndef SIMULATOR
    static const uint16_t PARAM_LINE_SIZE = 40;
    touchgfx::Unicode::UnicodeChar paramLineBuffer[PARAM_LINE_SIZE];
    touchgfx::TextAreaWithOneWildcard paramLineText;
    void initParamLineText();
#endif
};

#endif // SCREENMENUVIEW_HPP
