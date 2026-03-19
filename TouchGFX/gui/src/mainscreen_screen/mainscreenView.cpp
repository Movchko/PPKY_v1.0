#include <gui/mainscreen_screen/mainscreenView.hpp>

mainscreenView::mainscreenView()
{

}

void mainscreenView::setupScreen()
{
    mainscreenViewBase::setupScreen();

    /* По умолчанию пустая бегущая строка, будет задаваться из приложения (пожар, ошибки и т.п.) */
    CustomContainerSrollText.setText("");
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

};

void mainscreenView::updateFireStatus(bool active, uint8_t zone, uint8_t remaining_s, const char* zoneName)
{
	if (!active) {
		// Сбрасываем textArea1 и бегущую строку
		for (uint16_t i = 0; i < TEXTAREA1_SIZE; i++) {
			textArea1Buffer[i] = 0;
		}
		Unicode::snprintf(textArea1Buffer, TEXTAREA1_SIZE, "%s", "");
		textArea1.setWildcard(textArea1Buffer);
		textArea1.invalidate();
		CustomContainerSrollText.setText("");
		return;
	}

	/* TextAreaWithOneWildcard использует фиксированный буфер.
	 * При коротких строках (например "29" после "30") в конце мог остаться хвост от прошлого значения.
	 * Обнуляем весь буфер перед записью, чтобы хвост не отображался как '?' */
	for (uint16_t i = 0; i < TEXTAREA1_SIZE; i++) {
		textArea1Buffer[i] = 0;
	}

	// textArea1: полностью управляем строкой через wildcard,
	// чтобы не зависеть от шаблона designer'а (там стоит "<>").
	char buf[16];
	if (remaining_s > 0) {
		snprintf(buf, sizeof(buf), "ПОЖАР %u", (unsigned)remaining_s);
	} else {
		snprintf(buf, sizeof(buf), "%s", "ПОЖАР");
	}
	Unicode::fromUTF8(reinterpret_cast<const uint8_t*>(buf), textArea1Buffer, TEXTAREA1_SIZE);
	textArea1Buffer[TEXTAREA1_SIZE - 1] = 0;
	textArea1.setWildcard(textArea1Buffer);
	textArea1.invalidate();

	// Бегущая строка: только имя зоны (если есть)
	const char* name = (zoneName != nullptr) ? zoneName : "";
	CustomContainerSrollText.setText(name);
}
#endif
