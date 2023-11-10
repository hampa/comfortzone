#include "plugin.hpp"
#include "grooves.h"
#include <math.h>

#define LERP(a, b, f) (a * (1.0f - f)) + (b * f)
#define LOG0001 -9.210340371976182f // -80dB
#define CLAMP(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))
#define NUM_PARAMS (16*NUM_INST)

struct GroveBox : Module {
	enum ParamIds {
		PATTERN_PARAM
	};

	enum InputIds {
		CLOCK_INPUT,
		RESET_INPUT,
		NUM_INPUTS
	};

	enum OutputIds {
		GATE_BASS_OUTPUT,
		VEL_BASS_OUTPUT,
		GATE_SNARE_OUTPUT,
		VEL_SNARE_OUTPUT,
		GATE_CLOSED_HIHAT_OUTPUT,
		VEL_CLOSED_HIHAT_OUTPUT,
		GATE_OPEN_HIHAT_OUTPUT,
		VEL_OPEN_HIHAT_OUTPUT,
		NUM_OUTPUTS
	};

	enum LightIds {
		LIGHT0_LIGHT,
		LIGHT1_LIGHT,
		NUM_LIGHTS
	};

	GroveBox () {
		
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		for (int i = 0; i < NUM_PARAMS; i++) {
			configParam(i, 0, 1.0f, 0.0f, "k", "", 0.f, 127.0f);
		}
		configInput(CLOCK_INPUT, "Clock Input");
		configInput(RESET_INPUT, "Reset Input");
	}

	~GroveBox () {
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override {
	}

	int current_step = 0;
	float prev_clock_input = -2;
	float prev_reset_input = -2;
	int gateon[NUM_INST];
	float amp[NUM_INST];
	float vel[NUM_INST];

	inline float fmap_linear(float in, float min, float max) {
		return CLAMP(min + in * (max - min), min, max);
	}

	inline float fmap_exp(float in, float min, float max) {
		return CLAMP(min + (in * in) * (max - min), min, max);
	}

	inline float fmap_log(float in, float min, float max) {
		const float a = 1.f / log10f(max / min);
		return CLAMP(min * powf(10, in / a), min, max);
        }

	void process(const ProcessArgs& args) override {
		float clock_input = inputs[CLOCK_INPUT].getVoltage(0);
		float reset_input = inputs[RESET_INPUT].getVoltage(0);

		bool have_clock = false;
		if (prev_clock_input < 1 && clock_input > 1) {
			have_clock = true;
			current_step++;
			if (current_step >= 128) {
				current_step = 0;
			}
		}
		if (prev_reset_input < 1 && reset_input > 1) {
			current_step = 0;
		}

		if (have_clock) {
			for (int i = 0; i < NUM_INST; i++) {
				float v = grooves[i][current_step];
				if (v > 0) {
					gateon[i] = 30;
					if (v > 1) {
						v /= 127.0f;
					}	
					vel[i] = v * 5.0f;
				}
				else {
					gateon[i] = 0;
				}
			}
		}
		else {
			for (int i = 0; i < NUM_INST; i++) {
				if (gateon[i] > 1) {
					gateon[i]--;
					amp[i] = 5;
				}
				else {
					gateon[i] = 0;
					amp[i] = 0;
				}
			}
		}

		outputs[GATE_BASS_OUTPUT].setVoltage(amp[INST_BASS]);
		outputs[GATE_SNARE_OUTPUT].setVoltage(amp[INST_SNARE]);
		outputs[GATE_OPEN_HIHAT_OUTPUT].setVoltage(amp[INST_OPEN_HIHAT]);
		outputs[GATE_CLOSED_HIHAT_OUTPUT].setVoltage(amp[INST_CLOSED_HIHAT]);

		outputs[VEL_BASS_OUTPUT].setVoltage(vel[INST_BASS]);
		outputs[VEL_SNARE_OUTPUT].setVoltage(vel[INST_SNARE]);
		outputs[VEL_OPEN_HIHAT_OUTPUT].setVoltage(vel[INST_OPEN_HIHAT]);
		outputs[VEL_CLOSED_HIHAT_OUTPUT].setVoltage(vel[INST_CLOSED_HIHAT]);
		

		prev_clock_input = clock_input;
		prev_reset_input = reset_input;
	}
};

struct GroveBoxWidget : ModuleWidget {
	Label* labelPitch;

	GroveBoxWidget(GroveBox* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/groovebox.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));

		float spacingX = 30; //box.size.x / (float)4.0f;
		int x0 = spacingX;
		//int x1 = box.size.x - spacingX;
		int x1 = x0 + spacingX;
		int x2 = x1 + spacingX;

		float y = 72;
		addInput(createInputCentered<PJ301MPort>(Vec(x0, y), module, GroveBox::CLOCK_INPUT));
		addInput(createInputCentered<PJ301MPort>(Vec(x1, y), module, GroveBox::RESET_INPUT));

		y = 142;
		//addParam(createParamCentered<RoundBlackKnob>(Vec(x0, y), module, GroveBox::EVEN_DETUNE_PARAM));
		//addParam(createParamCentered<RoundBlackKnob>(Vec(x1, y), module, GroveBox::ODD_DETUNE_PARAM));

		y = 190;
		addOutput(createOutputCentered<PJ301MPort>(Vec(x0, y), module, GroveBox::GATE_OPEN_HIHAT_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(x1, y), module, GroveBox::VEL_OPEN_HIHAT_OUTPUT));

		y = 238;
		addOutput(createOutputCentered<PJ301MPort>(Vec(x0, y), module, GroveBox::GATE_CLOSED_HIHAT_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(x1, y), module, GroveBox::VEL_CLOSED_HIHAT_OUTPUT));

		y = 296;
		addOutput(createOutputCentered<PJ301MPort>(Vec(x0, y), module, GroveBox::GATE_SNARE_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(x1, y), module, GroveBox::VEL_SNARE_OUTPUT));

		y = 344;
		addOutput(createOutputCentered<PJ301MPort>(Vec(x0, y), module, GroveBox::GATE_BASS_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(x1, y), module, GroveBox::VEL_BASS_OUTPUT));
		//addInput(createInputCentered<PJ301MPort>(Vec(x0, y), module, GroveBox::CLOCK_INPUT));
		//addInput(createInputCentered<PJ301MPort>(Vec(x1, y), module, GroveBox::RESET_INPUT));
		//addOutput(createOutputCentered<PJ301MPort>(Vec(x1, y), module, GroveBox::RIGHT_OUTPUT));
		y = 72;
		for (int i = 0; i < 16; i++) {
			int x = x2 + i * spacingX;
			addParam(createParamCentered<BefacoTinyKnob>(Vec(x, y), module, i));
		}

		labelPitch = createWidget<Label>(Vec(5, 90));
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
		/*
		GroveBox * module = dynamic_cast<GroveBox*>(this->module);
		if (module) {
			labelPitch->text = module->getRootNoteName();
		}
		*/
		ModuleWidget::step();
	}
};

Model* modelGroveBox = createModel<GroveBox, GroveBoxWidget>("grovebox");
