#ifndef SCREENMENUVIEW_HPP
#define SCREENMENUVIEW_HPP

#include <gui_generated/screenmenu_screen/ScreenMenuViewBase.hpp>
#include <gui/screenmenu_screen/ScreenMenuPresenter.hpp>

class ScreenMenuView : public ScreenMenuViewBase
{
public:
    ScreenMenuView();
    virtual ~ScreenMenuView() {}
    virtual void setupScreen();
    virtual void tearDownScreen();
#ifndef SIMULATOR
    virtual void SetupMenuChangePos(uint8_t val);
    virtual void scrollWheel1UpdateItem(mainmenu& item, int16_t itemIndex);
    virtual void scrollWheel1_1UpdateItem(mainmenu& item, int16_t itemIndex);
#endif
protected:
};

#endif // SCREENMENUVIEW_HPP
