#include "plugin.hpp"
#include "kickbass.h"


struct Kickbaba : Module {
	enum ParamIds {
		KICKPITCHMIN_PARAM,
		KICKPITCHMAX_PARAM,
		KICKPITCH_X1_PARAM,
		KICKPITCH_Y1_PARAM,
		BASS_PITCH_PARAM,
		SAW_PARAM,
		MORPH_PARAM,
		BASSVEL1_PARAM,
#ifdef DEBUG_STUFF
		KICKPITCH_X2_PARAM,
		KICKPITCH_Y2_PARAM,
		BASS_PHASE_PARAM,
		FREQ_PARAM,
		RES_PARAM,
#endif
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
		GATE_OUTPUT,
		START_OUTPUT,
#ifdef DEBUG_STUFF
		BASS_ENV_OUTPUT,
		BASS_RAW_OUTPUT,
		PITCH_ENV_OUTPUT,
		PITCH_OUTPUT,
#endif
		NUM_OUTPUTS
	};
	enum LightIds {
		LIGHT0_LIGHT,
		LIGHT1_LIGHT,
		LIGHT2_LIGHT,
		LIGHT3_LIGHT,
		NUM_LIGHTS
	};


	KickBass kickBass;
	Kickbaba() {
		kickBass.Init();

		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configParam(KICKPITCHMIN_PARAM, 0.f, 1.f, 0.f, "");
		configParam(KICKPITCHMAX_PARAM, 0.f, 1.f, 0.f, "");
		configParam(KICKPITCH_X1_PARAM, 0.f, 1.f, 0.f, "");
		configParam(KICKPITCH_Y1_PARAM, 0.f, 1.f, 0.f, "");
		//configParam(KICKPITCH_X2_PARAM, 0.f, 1.f, 0.f, "");
		//configParam(KICKPITCH_Y2_PARAM, 0.f, 1.f, 0.f, "");
		//configParam(FREQ_PARAM, 0.f, 1.f, 0.5f, "Frequency", " Hz", std::pow(2, 10.f), dsp::FREQ_C4 / std::pow(2, 5.f));
		//float freqMin = std::log2(ripples::kFreqKnobMin);
		//float freqMax = std::log2(ripples::kFreqKnobMax);
	//	configParam(FREQ_PARAM, freqMin, freqMax, freqMax, "Frequency", " Hz", std::pow(2, 10.f), dsp::FREQ_C4 / std::pow(2, 5.f));
		//configParam(FREQ_PARAM, freqMin, freqMax, freqMax, "Frequency", " Hz", 2.f); 
		//configParam(RES_PARAM, 0.f, 1.f, 0.f, "Resonance", "%", 0.f, 100.f);

		configParam(BASS_PITCH_PARAM, 0.f, 1.f, 0.f, "");
		configParam(SAW_PARAM, 0.f, 1.f, 0.f, "");
		configParam(MORPH_PARAM, 0.f, 1.f, 0.f, "");
		//configParam(BASS_PHASE_PARAM, 0.f, 1.f, 0.f, "");
		configParam(BASSVEL1_PARAM, 0.f, 1.f, 0.f, "");

	}

	~Kickbaba() {
		kickBass.Destroy();
	}

	dsp::SchmittTrigger clockTrig, resetTrig;
	dsp::PulseGenerator gateGenerator;

	void process(const ProcessArgs& args) override {
		float sample_time = args.sampleTime;
		float sample_rate = args.sampleRate;

		bool rst = resetTrig.process(inputs[RESET_INPUT].getVoltage());
		bool clk = clockTrig.process(inputs[CLOCK_INPUT].getVoltage());
		float x1 = params[KICKPITCH_X1_PARAM].getValue();
		float y1 = params[KICKPITCH_Y1_PARAM].getValue();
		float x2 = 0; //params[KICKPITCH_X2_PARAM].getValue();
		float y2 = 0; //params[KICKPITCH_Y2].getValue();

		float pitchVoltage = inputs[PITCH_INPUT].getVoltage();

		float freqParam = 0; //params[FREQ_PARAM].getValue();
		float resParam = 0; //params[RES_PARAM].getValue();
		float morphParam = params[MORPH_PARAM].getValue();
		float bassPitchParam = params[BASS_PITCH_PARAM].getValue();
		float bassPhaseParam = 0; //params[BASS_PHASE_PARAM].getValue();
		float sawParam = params[SAW_PARAM].getValue();
		float kickPitchMinParam = params[KICKPITCHMIN_PARAM].getValue();
		float kickPitchMaxParam = params[KICKPITCHMAX_PARAM].getValue();
		float bassVelParam = params[BASSVEL1_PARAM].getValue();


		kickBass.process(sample_time, sample_rate, clk, rst, x1, y1, x2, y2,
				pitchVoltage,
				freqParam,
				resParam,
				sawParam,
				morphParam,
				kickPitchMinParam,
				kickPitchMaxParam,
				bassVelParam,
				bassPitchParam,
				bassPhaseParam);

		outputs[KICK_OUTPUT].setVoltage(kickBass.outputs[KickBass::KICK_OUT] * 5.0f);
		outputs[BASS_OUTPUT].setVoltage(kickBass.outputs[KickBass::BASS_OUT]);
		outputs[LFO_OUTPUT].setVoltage(kickBass.outputs[KickBass::LFO_OUT] * 8.0f);
		outputs[GATE_OUTPUT].setVoltage(kickBass.outputs[KickBass::GATE_OUT] * 8.0f);
		outputs[START_OUTPUT].setVoltage(kickBass.outputs[KickBass::START_OUT] * 8.0f);

		//outputs[BASS_ENV_OUTPUT].setVoltage(kickBass.outputs[KickBass::BASS_ENV_OUT] * 8.0f);
		//outputs[BASS_RAW_OUTPUT].setVoltage(kickBass.outputs[KickBass::BASS_RAW_OUT] * 8.0f);
		//outputs[PITCH_ENV_OUTPUT].setVoltage(kickBass.outputs[KickBass::PITCH_ENV_OUT] * 8.0f);
		//outputs[PITCH_OUTPUT].setVoltage(kickBass.outputs[KickBass::PITCH_OUT]);

		if (kickBass.gateTrigger) {
			gateGenerator.trigger();
		}

		kickBass.postProcess();
	}
};


struct KickbabaWidget : ModuleWidget {
	Label* label; 
	Label* labelPitch;
	Label* labelKickPitch;
	Label* labelKickSweep;

	KickbabaWidget(Kickbaba* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/kickbaba10HP.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		//float spacingX = (RACK_GRID_WIDTH * 12.0f) / 8.0f;
		float spacingX = box.size.x / (float)8.0f; 

		//int spacingX = 24;
		float y = 130;
		int w = spacingX * 2.0f;
		int x0 = spacingX;
		int x1 = box.size.x / 2 - spacingX;
		int x2 = box.size.x / 2 + spacingX; 
		int x3 = box.size.x - spacingX;
		int x4 = x3 + w;
		int x5 = x4 + w;


		// kick 
		addParam(createParamCentered<RoundBlackKnob>(Vec(x0, y), module, Kickbaba::KICKPITCHMIN_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(Vec(x1, y), module, Kickbaba::KICKPITCHMAX_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(Vec(x2, y), module, Kickbaba::KICKPITCH_X1_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(Vec(x3, y), module, Kickbaba::KICKPITCH_Y1_PARAM));

		//addParam(createParam<RoundBlackKnob>(Vec(x4 + 100, y), module, Kickbaba::KICKPITCH_X2_PARAM));

		// bass
		y = 201;
		addParam(createParamCentered<RoundBlackKnob>(Vec(x0, y), module, Kickbaba::BASS_PITCH_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(Vec(x1, y), module, Kickbaba::BASSVEL1_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(Vec(x2, y), module, Kickbaba::SAW_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(Vec(x3, y), module, Kickbaba::MORPH_PARAM));

		//addParam(createParam<RoundBlackKnob>(Vec(x3 + 100, y), module, Kickbaba::FREQ_PARAM));
		//addParam(createParam<RoundBlackKnob>(Vec(x4, y), module, Kickbaba::RES_PARAM));
		//addParam(createParam<RoundBlackKnob>(Vec(x4 + 100, y), module, Kickbaba::BASS_PHASE_PARAM));

		int iow = 34;
		int iox0 = 12;
		int iox1 = iox0 + iow;
		int iox2 = iox1 + iow;
		int iox3 = iox2 + iow;
		int iox4 = iox3 + iow;
		int iox5 = iox4 + iow;
		int iox6 = iox5 + iow;
		int iox7 = iox6 + iow;
		int iox8 = iox7 + iow;

		y = 272;
		addInput(createInputCentered<PJ301MPort>(Vec(x0, y), module, Kickbaba::PITCH_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(x1, y), module, Kickbaba::START_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(x2, y), module, Kickbaba::LFO_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(x3, y), module, Kickbaba::GATE_OUTPUT));

		y = 344;
		addInput(createInputCentered<PJ301MPort>(Vec(x0, y), module, Kickbaba::CLOCK_INPUT));
		addInput(createInputCentered<PJ301MPort>(Vec(x1, y), module, Kickbaba::RESET_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(x2, y), module, Kickbaba::KICK_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(x3, y), module, Kickbaba::BASS_OUTPUT));

			/*
		addOutput(createOutput<PJ301MPort>(Vec(iox5, y), module, Kickbaba::BASS_ENV_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(iox6, y), module, Kickbaba::BASS_RAW_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(iox7, y), module, Kickbaba::PITCH_ENV_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(iox8, y), module, Kickbaba::PITCH_OUTPUT));
			*/

		

		/*
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(131.125, 68.642)), module, Kickbaba::LIGHT0_LIGHT));
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(149.324, 68.642)), module, Kickbaba::LIGHT1_LIGHT));
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(166.825, 68.642)), module, Kickbaba::LIGHT2_LIGHT));
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(183.979, 70.034)), module, Kickbaba::LIGHT3_LIGHT));
		*/


		//nvgCreateFont(vg, "sans", ".\\res\\LEMONMILK-Light.otf");

		// RACK_GRID_WIDTH (px / HP)
		// box.size.x
		label = createWidget<Label>(Vec(0, 30));
		//DEBUG("rack width %f box size.x %f", RACK_GRID_WIDTH, box.size.x);
		label->box.size = Vec(box.size.x, RACK_GRID_WIDTH * 12);
		label->text = "Kick BaBa";
		addChild(label);

		labelPitch = createWidget<Label>(Vec(5, 216));
		labelPitch->box.size = Vec(50, 50);
		labelPitch->text = "#F";
		addChild(labelPitch);

		labelKickPitch = createWidget<Label>(Vec(5, 145));
		labelKickPitch->box.size = Vec(50, 50);
		labelKickPitch->text = "C";
		addChild(labelKickPitch);

		labelKickSweep = createWidget<Label>(Vec(45, 145));
		labelKickSweep->box.size = Vec(50, 50);
		labelKickSweep->text = "C";
		addChild(labelKickSweep);

	}

	void step() override {
		Kickbaba* module = dynamic_cast<Kickbaba*>(this->module);
		if (module) {
			char buf[128];
			label->color = nvgRGBA(0, 0, 0, 255);
			snprintf(buf, sizeof(buf), "BAR: %s\nK: %s\nB: %s",
					module->kickBass.getBarInfo(),
					module->kickBass.getKickInfo(),
					module->kickBass.getBassInfo());
			label->text = buf;

			labelPitch->color = nvgRGBA(255, 255, 255, 127);
			labelPitch->text = module->kickBass.getBassNoteName();

			labelKickPitch->color = nvgRGBA(255, 255, 255, 127);
			labelKickPitch->text = module->kickBass.getKickNoteName();

			labelKickSweep->color = nvgRGBA(255, 255, 255, 127);
			labelKickSweep->text = module->kickBass.getKickSweepNoteName();
		}
		ModuleWidget::step();
	}

	void draw(const DrawArgs &args) override {
		ModuleWidget::draw(args);
		if (module == NULL) {
			return;
		}
		drawGraph(args.vg, 0, 30, RACK_GRID_WIDTH * 10, 60, 1.0f);
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

		/*
		// font
		nvgFillColor(vg, nvgRGBA(255,255,255,255));
		nvgFontSize(vg, 10);
		nvgText(vg, 0, 220, "Hello", NULL);
		*/

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
