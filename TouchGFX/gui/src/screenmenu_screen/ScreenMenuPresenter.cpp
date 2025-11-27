#include <gui/screenmenu_screen/ScreenMenuView.hpp>
#include <gui/screenmenu_screen/ScreenMenuPresenter.hpp>

ScreenMenuPresenter::ScreenMenuPresenter(ScreenMenuView& v)
    : view(v)
{

}

void ScreenMenuPresenter::activate()
{

}

void ScreenMenuPresenter::deactivate()
{

}
#ifndef SIMULATOR
void ScreenMenuPresenter::SetupMenuChangePos(unsigned char val) {
	view.SetupMenuChangePos(val);

};
#endif
