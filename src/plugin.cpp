#include "plugin.hpp"

Plugin* pluginInstance;
extern Model* modelKickbaba;
extern Model* modelFmlead;
extern Model* modelSsqlead;
extern Model* modelTzfmlead;

void init(Plugin* p) {
	pluginInstance = p;
	p->addModel(modelSsqlead);
	p->addModel(modelKickbaba);
	p->addModel(modelFmlead);
	p->addModel(modelTzfmlead);
}
