#include "plugin.hpp"

Plugin* pluginInstance;

void init(Plugin* p) {
	pluginInstance = p;
	p->addModel(modelAtmos714);
	p->addModel(modelIslandRoom);
}
