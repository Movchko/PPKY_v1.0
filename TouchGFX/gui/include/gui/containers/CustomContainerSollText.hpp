#ifndef CUSTOMCONTAINERSOLLTEXT_HPP
#define CUSTOMCONTAINERSOLLTEXT_HPP

#include <gui_generated/containers/CustomContainerSollTextBase.hpp>
#include <touchgfx/widgets/TextAreaWithWildcard.hpp>
#include <touchgfx/Unicode.hpp>

class CustomContainerSollText : public CustomContainerSollTextBase
{
public:
    typedef void (*FinishedCallback)(CustomContainerSollText* sender);

    CustomContainerSollText();
    virtual ~CustomContainerSollText() {}

    virtual void initialize();

    /**
     * Задать текст бегущей строки (ASCII/UTF-8, не более 64 символов).
     */
    void setText(const char* text);

    /**
     * Установить колбэк, который вызывается один раз
     * после полной прокрутки текущего текста.
     * Если колбэк не задан — бегущая строка автоматически запускается заново.
     */
    void setFinishedCallback(FinishedCallback cb) { finishedCallback = cb; }

    /**
     * Принудительно запустить бегущую строку заново с начала (текущий текст).
     */
    void restart();

    virtual void handleTickEvent() override;

protected:
private:
    static const uint16_t MARQUEE_BUFFER_SIZE = 65; // 64 символа + терминатор

    touchgfx::TextAreaWithOneWildcard marqueeText;
    touchgfx::Unicode::UnicodeChar marqueeBuffer[MARQUEE_BUFFER_SIZE];

    int16_t marqueeX;
    int16_t marqueeTextWidth;
    bool marqueeRunning;
    int32_t frameCountInteraction1Interval;
    int32_t delayframeCountInteraction1Interval;
    FinishedCallback finishedCallback;
};

#endif // CUSTOMCONTAINERSOLLTEXT_HPP
