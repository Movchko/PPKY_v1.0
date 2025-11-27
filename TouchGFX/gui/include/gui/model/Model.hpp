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

    void tick();
protected:
    ModelListener* modelListener;
};

#endif // MODEL_HPP
