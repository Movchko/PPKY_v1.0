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
#endif

protected:
    ModelListener* modelListener;

    bool soundOn;

#ifndef SIMULATOR
    SoundToggledCallback soundToggledCallback;
#endif
};

#endif // MODEL_HPP
