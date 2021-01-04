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
		BASS_VEL_PARAM,
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
		configParam(BASS_PITCH_PARAM, 0.f, 1.f, 0.f, "");
		configParam(SAW_PARAM, 0.f, 1.f, 0.f, "");
		configParam(MORPH_PARAM, 0.f, 1.f, 0.f, "");
		configParam(BASS_VEL_PARAM, 0.f, 1.f, 0.f, "");

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
		float x2 = 0;
		float y2 = 0;
		float pitchVoltage = inputs[PITCH_INPUT].getVoltage();
		float freqParam = 0;
		float resParam = 0;
		float morphParam = params[MORPH_PARAM].getValue();
		float bassPitchParam = params[BASS_PITCH_PARAM].getValue();
		float bassPhaseParam = 0;
		float sawParam = params[SAW_PARAM].getValue();
		float kickPitchMinParam = params[KICKPITCHMIN_PARAM].getValue();
		float kickPitchMaxParam = params[KICKPITCHMAX_PARAM].getValue();
		float bassVelParam = params[BASS_VEL_PARAM].getValue();


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

		if (kickBass.gateTrigger) {
			gateGenerator.trigger();
		}

		kickBass.postProcess();
	}
};


struct KickbabaWidget : ModuleWidget {
	//Label* label; 
	Label* labelPitch;
	Label* labelKickPitch;
	Label* labelKickSweep;
	Label* labelPhase;

	KickbabaWidget(Kickbaba* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/kickbaba10HP.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		//addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		//addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		float spacingX = box.size.x / (float)8.0f; 
		float y = 95;
		int x0 = spacingX;
		int x1 = box.size.x / 2 - spacingX;
		int x2 = box.size.x / 2 + spacingX; 
		int x3 = box.size.x - spacingX;

		addParam(createParamCentered<RoundBlackKnob>(Vec(x0, y), module, Kickbaba::KICKPITCHMIN_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(Vec(x1, y), module, Kickbaba::KICKPITCHMAX_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(Vec(x2, y), module, Kickbaba::KICKPITCH_X1_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(Vec(x3, y), module, Kickbaba::KICKPITCH_Y1_PARAM));

		y = 201;
		addParam(createParamCentered<RoundBlackKnob>(Vec(x0, y), module, Kickbaba::BASS_PITCH_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(Vec(x1, y), module, Kickbaba::BASS_VEL_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(Vec(x2, y), module, Kickbaba::SAW_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(Vec(x3, y), module, Kickbaba::MORPH_PARAM));

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

		labelPhase = createWidget<Label>(Vec(65, 132));
		labelPhase->box.size = Vec(50, 50);
		labelPhase->text = "0.00";
		labelPhase->color = metallic4;
		addChild(labelPhase);

		labelKickPitch = createWidget<Label>(Vec(5, 110));
		labelKickPitch->box.size = Vec(50, 50);
		labelKickPitch->text = "C";
		labelKickPitch->color = metallic4; 
		addChild(labelKickPitch);

		labelKickSweep = createWidget<Label>(Vec(45, 110));
		labelKickSweep->box.size = Vec(50, 50);
		labelKickSweep->text = "C";
		labelKickSweep->color = metallic4; 
		addChild(labelKickSweep);

		labelPitch = createWidget<Label>(Vec(5, 216));
		labelPitch->box.size = Vec(50, 50);
		labelPitch->text = "#F";
		labelPitch->color = metallic4; 
		addChild(labelPitch);
	}

	// color scheme Echo Icon Theme Palette in Inkscape
	NVGcolor metallic2 = nvgRGBA(158, 171, 176, 255);
	NVGcolor metallic4 = nvgRGBA(14, 35, 46, 255);
	NVGcolor green1 = nvgRGBA(204, 255, 66, 255);
	NVGcolor red1 = nvgRGBA(255, 65, 65, 255);

	void step() override {
		Kickbaba* module = dynamic_cast<Kickbaba*>(this->module);
		if (module) {
			/*
			char buf[128];
			label->color = nvgRGBA(204, 204, 204, 255);
			snprintf(buf, sizeof(buf), "BAR: %s\nK: %s\nB: %s",
					module->kickBass.getBarInfo(),
					module->kickBass.getKickInfo(),
					module->kickBass.getBassInfo());
			label->text = buf;
			*/

			labelPhase->text = module->kickBass.getPhaseInfo();
			labelPitch->text = module->kickBass.getBassNoteName();
			labelKickPitch->text = module->kickBass.getKickNoteName();
			labelKickSweep->text = module->kickBass.getKickSweepNoteName();
		}
		ModuleWidget::step();
	}

	void draw(const DrawArgs &args) override {
		ModuleWidget::draw(args);
		if (module == NULL) {
			return;
		}
		drawGraph(args.vg, 80, 220, 70, 10, 1.0f);
	}

	const int graphResolution = 50;
	void drawGraph(NVGcontext* vg, float x, float y, float w, float h, float t)
	{
		float sx[graphResolution], sy[graphResolution];
		int i;

		// display background
		/*
		nvgBeginPath(vg);
		nvgRect(vg, x, y, w, h);
		nvgFillColor(vg, nvgRGBA(0,160,192,128));
		nvgFill(vg);
		*/

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
			sx[i] = x + px * w;
		}

		nvgBeginPath(vg);
		nvgMoveTo(vg, sx[0], sy[0]);
		for (i = 1; i < graphResolution; i++) {
			nvgLineTo(vg, sx[i], sy[i]);
		}
		nvgStrokeColor(vg, metallic2);
		nvgStrokeWidth(vg, 1.0f);
		nvgStroke(vg);

		int xStart = 45;
		int yStart = 290;
		int barWidth = 6;

		nvgBeginPath(vg);
		nvgMoveTo(vg, xStart, yStart);
	      	nvgLineTo(vg, xStart + module->kickBass.getBar() * barWidth, yStart);
		nvgStrokeColor(vg, red1);
		nvgStrokeWidth(vg, 1.0f);
		nvgStroke(vg);	

		/*
		point2 p;
		for (int i = 0; i < graphResolution; i++) {
			module->kickBass.getKickPitchGraph(i / (float)graphResolution, &p);
			sy[i] = (1.0f - p.y) * h + y;
			sx[i] = 5 + p.x * w;
		}

		nvgBeginPath(vg);
		nvgMoveTo(vg, sx[0], sy[0]);
		for (i = 1; i < graphResolution; i++) {
			nvgLineTo(vg, sx[i], sy[i]);
		}
		nvgStrokeColor(vg, red1);
		nvgStrokeWidth(vg, 1.0f);
		nvgStroke(vg);
		*/

	}
};

Model* modelKickbaba = createModel<Kickbaba, KickbabaWidget>("kickbaba");
