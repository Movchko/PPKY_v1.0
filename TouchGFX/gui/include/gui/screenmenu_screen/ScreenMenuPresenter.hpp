#ifndef SCREENMENUPRESENTER_HPP
#define SCREENMENUPRESENTER_HPP

#include <gui/model/ModelListener.hpp>
#include <mvp/Presenter.hpp>

using namespace touchgfx;

class ScreenMenuView;

class ScreenMenuPresenter : public touchgfx::Presenter, public ModelListener
{
public:
    ScreenMenuPresenter(ScreenMenuView& v);

    virtual void activate();
    virtual void deactivate();

    virtual ~ScreenMenuPresenter() {}
#ifndef SIMULATOR
    virtual void SetupMenuChangePos(unsigned char val);
    virtual void handleButton(uint8_t but, uint8_t state) override;
#endif
private:
    ScreenMenuPresenter();

    ScreenMenuView& view;

#ifndef SIMULATOR
    static const int MENU_ITEMS = 6;
    bool soundOn;
    int16_t currentIndex;
    void refreshLine();
#endif
};

#endif // SCREENMENUPRESENTER_HPP
