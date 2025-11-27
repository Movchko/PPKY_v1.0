#include <gui/model/Model.hpp>
#include <gui/model/ModelListener.hpp>

Model::Model() : modelListener(0)
{

}
unsigned char pos = 0;
void Model::tick()
{
#ifndef SIMULATOR
	if(setup_change) {
		setup_change = 0;
		modelListener->SetupMenuChangePos(pos++);
		if(pos >= 6)
			pos = 0;
	}
#endif
}
