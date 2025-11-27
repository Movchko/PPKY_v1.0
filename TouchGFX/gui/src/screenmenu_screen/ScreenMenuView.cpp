#include <gui/screenmenu_screen/ScreenMenuView.hpp>
#include <cstdio>
ScreenMenuView::ScreenMenuView()
{

}

void ScreenMenuView::setupScreen()
{
    ScreenMenuViewBase::setupScreen();
#ifndef SIMULATOR
    for (int i = 0; i < scrollWheel1ListItems.getNumberOfDrawables(); i++)
    {
        scrollWheel1.itemChanged(i);
        scrollWheel1ListItems[i].updateText(i);
    }
#endif
}

void ScreenMenuView::tearDownScreen()
{
    ScreenMenuViewBase::tearDownScreen();
}
#ifndef SIMULATOR
void ScreenMenuView::SetupMenuChangePos(uint8_t val) {


	if (val >= 0 && val < scrollWheel1.getNumberOfItems())
		scrollWheel1.animateToItem(val, -1);

	//scrollWheel1.invalidate();
	//scrollWheel1.invalidateContent();
	//invalidate();


	//box1.invalidate();
	//scrollList1.animateToItem(val, 10);
	//scrollList1.invalidate();


/*
	if(val == 0)
		scrollableContainer2.reset();
	else {

		scrollableContainer2.doScroll(0, -val*64 );

	}

*/


	//int pos = scrollWheel1.getSelectedItem();




}

void ScreenMenuView::scrollWheel1UpdateItem(mainmenu& item, int16_t itemIndex)
{
    item.updateText(itemIndex);
}

/*
void ScreenMenuView::scrollList1UpdateItem(mainmenu& item, int16_t itemIndex) {
	switch (itemIndex) {
		case 0: {

		}break;
	}
}
*/

#endif
//void ScreenMenuView::scrollWheel1UpdateItem(CustomContainer3& item, int16_t itemIndex) {

//}
