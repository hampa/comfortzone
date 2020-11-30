#include "plugin.hpp"
#include "kickbass.h"

struct Kickbaba : Module {
	enum ParamIds {
		//PITCH_PARAM,
		BASSVEL1_PARAM,
		KICKPITCH_X1_PARAM,
		KICKPITCH_Y1_PARAM,
		KICKPITCH_X2_PARAM,
		KICKPITCH_Y2_PARAM,
		KICKPITCHMAX_PARAM,
		FREQ_PARAM,
		RES_PARAM,
		MORPH_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		RESET_INPUT,
		PITCH_INPUT,
		CLOCK_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		KICK_OUTPUT,
		BASS_OUTPUT,
		LFO_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		LIGHT0_LIGHT,
		LIGHT1_LIGHT,
		LIGHT2_LIGHT,
		LIGHT3_LIGHT,
		NUM_LIGHTS
	};

	// F1 = 43hz
	//  bottom frequency F on kickbass
	//  scorb kick
	//  6000 to 2000 quick
	//  kick attack, short OR nothing
	//  saw wave that is just 1 to 0
	//  attack 0 - 10 ms
	//  filter, no resonance 

	//Bezier::Bezier<3> *kickPitchBezier; //({ {0, 1}, {0.5, 0.5}, {0, 0}, {1, 0}});
	KickBass kickBass;
	Kickbaba() {
		kickBass.Init();

		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		//configParam(PITCH_PARAM, 0.f, 1.f, 0.f, "");
		configParam(BASSVEL1_PARAM, 0.f, 1.f, 0.f, "");
		configParam(KICKPITCH_X1_PARAM, 0.f, 1.f, 0.f, "");
		configParam(KICKPITCH_Y1_PARAM, 0.f, 1.f, 0.f, "");
		configParam(KICKPITCH_X2_PARAM, 0.f, 1.f, 0.f, "");
		configParam(KICKPITCH_Y2_PARAM, 0.f, 1.f, 0.f, "");
		configParam(KICKPITCHMAX_PARAM, 0.f, 1.f, 0.f, "");
		configParam(FREQ_PARAM, 0.f, 1.f, 0.5f, "Frequency", " Hz", std::pow(2, 10.f), dsp::FREQ_C4 / std::pow(2, 5.f));
		configParam(RES_PARAM, 0.f, 1.f, 0.f, "Resonance", "%", 0.f, 100.f);

		// dev
		configParam(MORPH_PARAM, 0.f, 1.f, 0.f, "");

	}

	~Kickbaba() {
		kickBass.Destroy();
	}

	dsp::SchmittTrigger clockTrig, resetTrig;
	dsp::PulseGenerator gateGenerator;

	void process(const ProcessArgs& args) override {
		float sample_time = args.sampleTime;
		float sample_rate = args.sampleRate;

		//DEBUG("%f %f", sample_rate, sample_time);

		bool rst = resetTrig.process(inputs[RESET_INPUT].getVoltage());
		bool clk = clockTrig.process(inputs[CLOCK_INPUT].getVoltage());
		float x1 = params[KICKPITCH_X1_PARAM].getValue();
		float y1 = params[KICKPITCH_Y1_PARAM].getValue();
		float x2 = params[KICKPITCH_X2_PARAM].getValue();
		float y2 = 0; //params[KICKPITCH_Y2].getValue();

		float pitchVoltage = inputs[PITCH_INPUT].getVoltage();

		float freqParam = params[FREQ_PARAM].getValue();
		float resParam = params[RES_PARAM].getValue();
		float morphParam = params[MORPH_PARAM].getValue();
		float kickPitchMaxParam = params[KICKPITCHMAX_PARAM].getValue();
		float bassVelParam = params[BASSVEL1_PARAM].getValue();

		kickBass.process(sample_time, sample_rate, clk, rst, x1, y1, x2, y2,
				pitchVoltage,
				freqParam,
				resParam,
				morphParam,
				kickPitchMaxParam,
				bassVelParam);

		outputs[KICK_OUTPUT].setVoltage(kickBass.outputs[KickBass::KICK_OUT]);
		outputs[BASS_OUTPUT].setVoltage(kickBass.outputs[KickBass::BASS_OUT]);
		outputs[LFO_OUTPUT].setVoltage(kickBass.outputs[KickBass::LFO_OUT]);

		if (kickBass.gateTrigger) {
			gateGenerator.trigger();
		}

		kickBass.postProcess();
	}
};


#define WITH_10HP 1
struct KickbabaWidget : ModuleWidget {
	Label* label; 
	KickbabaWidget(Kickbaba* module) {
		setModule(module);
#ifdef WITH_10HP
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/kickbababa10HP.svg")));
#else
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/psykickbass.svg")));
#endif

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		int y = 130;
		int w = 34;
		int x0 = 10;
		int x1 = x0 + w;
		int x2 = x1 + w;
		int x3 = x2 + w;

		// kick 
		y = 130;
		addParam(createParam<RoundBlackKnob>(Vec(x0, y), module, Kickbaba::KICKPITCHMAX_PARAM));
		addParam(createParam<RoundBlackKnob>(Vec(x1, y), module, Kickbaba::KICKPITCH_X1_PARAM));
		addParam(createParam<RoundBlackKnob>(Vec(x2, y), module, Kickbaba::KICKPITCH_Y1_PARAM));
		addParam(createParam<RoundBlackKnob>(Vec(x3, y), module, Kickbaba::KICKPITCH_X2_PARAM));

		// bass
		y = 205;
		addParam(createParam<RoundBlackKnob>(Vec(x0, y), module, Kickbaba::MORPH_PARAM));
		addParam(createParam<RoundBlackKnob>(Vec(x1, y), module, Kickbaba::FREQ_PARAM));
		addParam(createParam<RoundBlackKnob>(Vec(x2, y), module, Kickbaba::RES_PARAM));
		addParam(createParam<RoundBlackKnob>(Vec(x3, y), module, Kickbaba::BASSVEL1_PARAM));

		int iow = 34;
		int iox0 = 12;
		int iox1 = iox0 + iow;
		int iox2 = iox1 + iow;
		int iox3 = iox2 + iow;
		//int iox4 = iox3 + iow;

		addInput(createInput<PJ301MPort>(Vec(iox0, 268), module, Kickbaba::RESET_INPUT));
		addOutput(createOutput<PJ301MPort>(Vec(iox3, 268), module, Kickbaba::LFO_OUTPUT));

		// input output
		addInput(createInput<PJ301MPort>(Vec(iox0, 320), module, Kickbaba::CLOCK_INPUT));
		addInput(createInput<PJ301MPort>(Vec(iox1, 320), module, Kickbaba::PITCH_INPUT));
		addOutput(createOutput<PJ301MPort>(Vec(iox2, 320), module, Kickbaba::KICK_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(iox3, 320), module, Kickbaba::BASS_OUTPUT));


		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(131.125, 68.642)), module, Kickbaba::LIGHT0_LIGHT));
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(149.324, 68.642)), module, Kickbaba::LIGHT1_LIGHT));
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(166.825, 68.642)), module, Kickbaba::LIGHT2_LIGHT));
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(183.979, 70.034)), module, Kickbaba::LIGHT3_LIGHT));


		// RACK_GRID_WIDTH (px / HP)
		// box.size.x
		label = createWidget<Label>(Vec(0, 30));
		//DEBUG("rack width %f box size.x %f", RACK_GRID_WIDTH, box.size.x);
		label->box.size = Vec(box.size.x, 60);
		label->text = "Kick BaBa";
		addChild(label);
	}

	void step() override {
		Kickbaba* module = dynamic_cast<Kickbaba*>(this->module);
		if (module) {
			label->color = nvgRGBA(0, 0, 0, 255);

			label->text = 	"BAR : " + std::to_string(module->kickBass.getBar()) + "\n" + 
					"KICK: " + module->kickBass.getKickInfo() + "\n" + 
					"BASS: " + module->kickBass.getBassInfo();
		}
		ModuleWidget::step();
	}

	void draw(const DrawArgs &args) override {
		if (module == NULL) {
			return;
		}
		ModuleWidget::draw(args);
		drawGraph(args.vg, 0, 30, 150, 60, 1.0f);
	}

	const int graphResolution = 50;
	void drawGraph(NVGcontext* vg, float x, float y, float w, float h, float t)
	{
		float sx[graphResolution], sy[graphResolution];
		int i;

		// display background
		nvgBeginPath(vg);
		nvgRect(vg, x, y, w, h);
		nvgFillColor(vg, nvgRGBA(0,160,192,128));
		nvgFill(vg);

		Kickbaba* module = dynamic_cast<Kickbaba*>(this->module);
		if (module == NULL) {
			return;	
		}
		if (module->kickBass.isInitialized == 0)
			return;


		for (int i = 0; i < graphResolution; i++) {
			float px = i / (float)graphResolution;
			float py = module->kickBass.getBassGraph(px);
			sy[i] = (1.0f - py) * h + y;
			sx[i] = 2 + px * w;
		}

		nvgBeginPath(vg);
		nvgMoveTo(vg, sx[0], sy[0]);
		for (i = 1; i < graphResolution; i++) {
			nvgLineTo(vg, sx[i], sy[i]);
		}
		nvgStrokeColor(vg, nvgRGBA(255,0,0,80));
		nvgStrokeWidth(vg, 1.0f);
		nvgStroke(vg);

		point2 p;
		for (int i = 0; i < graphResolution; i++) {
			module->kickBass.getKickPitchGraph(i / (float)graphResolution, &p);
			sy[i] = (1.0f - p.y) * h + y;
			sx[i] = 2 + p.x * w;
		}

		nvgBeginPath(vg);
		nvgMoveTo(vg, sx[0], sy[0]);
		for (i = 1; i < graphResolution; i++) {
			nvgLineTo(vg, sx[i], sy[i]);
		}
		nvgStrokeColor(vg, nvgRGBA(255,255,0,80));
		nvgStrokeWidth(vg, 1.0f);
		nvgStroke(vg);

	}
};

Model* modelKickbaba = createModel<Kickbaba, KickbabaWidget>("kickbaba");

