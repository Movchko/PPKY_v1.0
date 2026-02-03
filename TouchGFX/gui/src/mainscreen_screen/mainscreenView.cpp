#include <gui/mainscreen_screen/mainscreenView.hpp>

mainscreenView::mainscreenView()
{

}

void mainscreenView::setupScreen()
{
    mainscreenViewBase::setupScreen();
}

void mainscreenView::tearDownScreen()
{
    mainscreenViewBase::tearDownScreen();
}

void mainscreenView::setDateTime(uint8_t hour, uint8_t min, uint8_t sec, uint8_t day, uint8_t month, uint8_t year)
{
    customContainerScrollTime1.setTime(hour, min, sec, day, month, year);
}

#ifndef SIMULATOR
void mainscreenView::SetTime(uint32_t time) {

	/*
	 * TODO: при наличии RTC распаковать time или получить время из модели и вызвать setDateTime(...)
	 */

};
#endif
