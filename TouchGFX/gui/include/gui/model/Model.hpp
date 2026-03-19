#ifndef MODEL_HPP
#define MODEL_HPP

#ifndef SIMULATOR
#include <main.h>
#endif


class ModelListener;

class Model
{
public:
    Model();

    void bind(ModelListener* listener)
    {
        modelListener = listener;
    }

    /** Текущий слушатель (привязанный презентер активного экрана) */
    ModelListener* getModelListener() { return modelListener; }

    void tick();

    /** Состояние параметра «звук включен/выключен» в модели */
    void setSoundOn(bool on) { soundOn = on; }
    bool getSoundOn() const { return soundOn; }

#ifndef SIMULATOR
    /** Колбэк: приложение узнаёт, что звук включён/выключен (сохранение — в приложении) */
    typedef void (*SoundToggledCallback)(bool soundOn);
    void setSoundToggledCallback(SoundToggledCallback cb) { soundToggledCallback = cb; }
    void notifySoundToggled(bool soundOn);

    /* Вызов из приложения ППКУ: обновить состояние пожара для UI.
     * active      - true, если пожар активен
     * zone        - номер зоны (0..ZONE_NUMBER-1) или 0xFF
     * remaining_s - оставшееся время до автозапуска тушения (сек), 0 если таймера нет
     * zoneName    - C-строка с именем зоны (UTF-8), может быть nullptr
     */
    void setFireStatusFromApp(bool active, uint8_t zone, uint8_t remaining_s, const char* zoneName);
#endif

protected:
    ModelListener* modelListener;

    bool soundOn;

#ifndef SIMULATOR
    SoundToggledCallback soundToggledCallback;

    bool fireActive = false;
    uint8_t fireZone = 0xFF;
    uint8_t fireRemaining = 0;
    char fireZoneName[65] = {0};
#endif
};

#endif // MODEL_HPP
