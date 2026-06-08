#ifndef SCREENMENU_JURNALVIEW_HPP
#define SCREENMENU_JURNALVIEW_HPP

#include <gui_generated/screenmenu_jurnal_screen/ScreenMenu_jurnalViewBase.hpp>
#include <gui/screenmenu_jurnal_screen/ScreenMenu_jurnalPresenter.hpp>

class ScreenMenu_jurnalView : public ScreenMenu_jurnalViewBase
{
public:
    ScreenMenu_jurnalView();
    virtual ~ScreenMenu_jurnalView() {}
    virtual void setupScreen();
    virtual void tearDownScreen();
protected:
};

#endif // SCREENMENU_JURNALVIEW_HPP
