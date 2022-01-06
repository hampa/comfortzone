#include "plugin.hpp"
#include "quantlookup.h"

struct Ssqlead : Module {
	enum ParamIds {
		PITCH_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		LEFT_INPUT,
		RIGHT_INPUT,
		TRIGGER_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		LEFT_OUTPUT,
		RIGHT_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		LIGHT0_LIGHT,
		LIGHT1_LIGHT,
		NUM_LIGHTS
	};

	#define BUFFER_SIZE 2048

	Ssqlead () {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(PITCH_PARAM, 0.f, 1.f, 0.f, "");
	}

	~Ssqlead () {
	}

	dsp::SchmittTrigger sampleTrig;
	int samples = 0;
	double instantaneous_w = 0;
	double current_phase = 0;
	double phase = 0;
	double w0 = 0;
	double w1 = 0;

	void process(const ProcessArgs& args) override {
		float sample_time = args.sampleTime;
		float sample_rate = args.sampleRate;

		double linear_phase, scaled, last_phase;
		double startFreq = 1;
		double endFreq = 4000;

		//w0 = 2.0f * M_PI * startFreq / 44100.0f;
		//w1 = 2.0f * M_PI * endFreq / 44100.0f;
		float pitch = params[PITCH_PARAM].getValue();

		if (samples == 0) {
			int idx = floor(pitch * 100.0f + 1.0f);
			startFreq = quants[idx];
			w0 = 2.0f * M_PI * startFreq / 44100.0f;
			w1 = 2.0f * M_PI * endFreq / 44100.0f;
			instantaneous_w = w0;
			current_phase = 0;
		}

		phase = (float)samples / BUFFER_SIZE;
		double out = sinf(current_phase) * 8.0;
		outputs[LEFT_OUTPUT].setVoltage(out);
		outputs[RIGHT_OUTPUT].setVoltage(out);

		float sweep = w0 + (w1 - w0) * phase;
		instantaneous_w = sweep;
		current_phase = fmod(current_phase + instantaneous_w, 2.0f * M_PI);

		samples++;
		if (samples >= BUFFER_SIZE) {
                        samples = 0;
			current_phase = 0;
			phase = 0;
                }
	}
};

struct SsqleadWidget : ModuleWidget {
	Label* labelPitch;
	Label* labelKickPitch;
	Label* labelKickSweep;
	Label* labelPhase;

	SsqleadWidget(Ssqlead* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/kickbaba10HP.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));


		float spacingX = box.size.x / (float)8.0f;
		float y = 95;
		int x0 = spacingX;
		int x1 = box.size.x / 2 - spacingX;
		int x2 = box.size.x / 2 + spacingX;
		int x3 = box.size.x - spacingX;

		addParam(createParamCentered<RoundBlackKnob>(Vec(x0, y), module, Ssqlead::PITCH_PARAM));

		y = 201;
		addInput(createInputCentered<PJ301MPort>(Vec(x0, y), module, Ssqlead::TRIGGER_INPUT));

		y = 272;
		addInput(createInputCentered<PJ301MPort>(Vec(x0, y), module, Ssqlead::LEFT_INPUT));
		addInput(createInputCentered<PJ301MPort>(Vec(x1, y), module, Ssqlead::RIGHT_INPUT));

		y = 344;
		addOutput(createOutputCentered<PJ301MPort>(Vec(x2, y), module, Ssqlead::LEFT_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(x3, y), module, Ssqlead::RIGHT_OUTPUT));

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

};

Model* modelSsqlead = createModel<Ssqlead, SsqleadWidget>("ssqlead");
