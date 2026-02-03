#ifndef MAINSCREENVIEW_HPP
#define MAINSCREENVIEW_HPP

#include <gui_generated/mainscreen_screen/mainscreenViewBase.hpp>
#include <gui/mainscreen_screen/mainscreenPresenter.hpp>

class mainscreenView : public mainscreenViewBase
{
public:
    mainscreenView();
    virtual ~mainscreenView() {}
    virtual void setupScreen();
    virtual void tearDownScreen();

    /**
     * Передать текущее время/дату в контейнер часов.
     */
    void setDateTime(uint8_t hour, uint8_t min, uint8_t sec, uint8_t day, uint8_t month, uint8_t year);

#ifndef SIMULATOR
    virtual void SetTime(uint32_t time);
#endif
protected:
};

#endif // MAINSCREENVIEW_HPP
