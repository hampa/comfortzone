#include "plugin.hpp"

Plugin* pluginInstance;
extern Model* modelKickbaba;
//extern Model* modelFmlead;
//extern Model* modelSsqlead;
extern Model* modelTzfmlead;
extern Model* modelSundial;
extern Model* modelGraintable;
extern Model* modelLazerBubbles;
extern Model* modelPartials;
extern Model* modelGator;

void init(Plugin* p) {
	pluginInstance = p;
	//p->addModel(modelSsqlead);
	p->addModel(modelKickbaba);
	//p->addModel(modelFmlead);
	p->addModel(modelTzfmlead);
	p->addModel(modelSundial);
	p->addModel(modelGraintable);
	p->addModel(modelLazerBubbles);
	p->addModel(modelPartials);
	p->addModel(modelGator);
}
