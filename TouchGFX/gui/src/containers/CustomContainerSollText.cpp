#include <gui/containers/CustomContainerSollText.hpp>
#include <touchgfx/Application.hpp>
#include <texts/TextKeysAndLanguages.hpp>

using namespace touchgfx;

CustomContainerSollText::CustomContainerSollText()
    : marqueeX(0),
      marqueeTextWidth(0),
      marqueeRunning(false),
      finishedCallback(0)
{
}

void CustomContainerSollText::initialize()
{
    CustomContainerSollTextBase::initialize();

    // Спрятать статический текст, заданный через Designer
    textAreatime.setVisible(false);
    textAreatime.invalidate();

    // Настроить нашу бегущую строку во всю ширину контейнера
    marqueeText.setPosition(0, textAreatime.getY(),
                            getWidth(), textAreatime.getHeight());
    marqueeText.setColor(textAreatime.getColor());
    marqueeText.setLinespacing(0);
    // Используем TypedText с wildcard, как в CustomContainerTime
    marqueeText.setTypedText(TypedText(T___SINGLEUSE_D1VE));
    marqueeText.setWildcard(marqueeBuffer);

    // Пустая строка по умолчанию
    marqueeBuffer[0] = 0;

    add(marqueeText);

    // Регистрируем виджет для получения тиков
    Application::getInstance()->registerTimerWidget(this);
    frameCountInteraction1Interval = 0;
}

void CustomContainerSollText::setText(const char* text)
{
    if (!text)
    {
        marqueeBuffer[0] = 0;
        marqueeText.invalidate();
        marqueeRunning = false;
        return;
    }

    // Переводим ASCII/UTF‑8 в Unicode буфер (ограничение 64 символа)
    Unicode::fromUTF8(reinterpret_cast<const uint8_t*>(text),
                      marqueeBuffer,
                      MARQUEE_BUFFER_SIZE);
    marqueeBuffer[MARQUEE_BUFFER_SIZE - 1] = 0;

    marqueeText.invalidate();
    marqueeText.resizeToCurrentText();
    marqueeTextWidth = marqueeText.getTextWidth();

    // Стартуем за правым краем видимой области контейнера
    marqueeX = 0;//getWidth();
    marqueeText.moveTo(marqueeX, marqueeText.getY());

    marqueeRunning = (marqueeTextWidth > 0);
    frameCountInteraction1Interval = 0;
    delayframeCountInteraction1Interval = 100;
}

void CustomContainerSollText::restart()
{
    if (marqueeTextWidth <= 0)
    {
        return;
    }
    marqueeX = 0;
    marqueeText.moveTo(marqueeX, marqueeText.getY());
    marqueeRunning = true;
    frameCountInteraction1Interval = 0;
    delayframeCountInteraction1Interval = 100;
}

void CustomContainerSollText::handleTickEvent()
{
    if (!marqueeRunning || marqueeTextWidth <= 0)
    {
        return;
    }

    if(delayframeCountInteraction1Interval) {
    	delayframeCountInteraction1Interval--;
    	return;
    }


    frameCountInteraction1Interval++;
    if(frameCountInteraction1Interval >= 4) {

		// Если весь текст уже вышел за левый край — завершение или автоповтор
		if (marqueeX + marqueeTextWidth <= 0)
		{
			if (finishedCallback)
			{
				marqueeRunning = false;
				finishedCallback(this);
			}
			else
			{
				// Колбэка нет — запускаем бегущую строку заново
				marqueeX = 0;
				marqueeText.moveTo(marqueeX, marqueeText.getY());
				delayframeCountInteraction1Interval = 100;
			}
			return;
		}

		// Двигаем текст влево на 1 пиксель за тик
		marqueeX--;
		marqueeText.moveTo(marqueeX, marqueeText.getY());
		frameCountInteraction1Interval = 0;
    }
}
