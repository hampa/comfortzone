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
extern Model* modelTrick;
extern Model* modelDevVco;
extern Model* modelDisperserSaw;
extern Model* modelGrainOsc;
extern Model* modelGroveBox;
//extern Model* modelEQ3;

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
	p->addModel(modelTrick);
	p->addModel(modelDevVco);
	p->addModel(modelDisperserSaw);
	p->addModel(modelGrainOsc);
	p->addModel(modelGroveBox);
	//p->addModel(modelEQ3);
}
