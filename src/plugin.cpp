#include "plugin.hpp"

Plugin* pluginInstance;
extern Model* modelKickbaba;

void init(Plugin* p) {
	pluginInstance = p;
	p->addModel(modelKickbaba);
}
