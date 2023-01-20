#include "plugin.hpp"
#include <math.h>

#define LERP(a, b, f) (a * (1.0f - f)) + (b * f)
#define LOG0001 -9.210340371976182f // -80dB
#define CLAMP(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

struct Gator : Module {
	enum ParamIds {
		DECAY_PARAM,
		BPM_PARAM,
		INFLUENCE_PARAM,
		NUM_PARAMS
	};

	enum InputIds {
		AUDIO_INPUT,
		CLOCK_INPUT,
		RESET_INPUT,
		INFLUENCE_INPUT,
		NUM_INPUTS
	};

	enum OutputIds {
		LEFT_OUTPUT,
		RIGHT_OUTPUT,
		STEPPED_RANDOM_OUTPUT,
		NUM_OUTPUTS
	};

	enum LightIds {
		LIGHT0_LIGHT,
		LIGHT1_LIGHT,
		NUM_LIGHTS
	};

	Gator () {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(DECAY_PARAM, 0.f, 1.f, 0.5f, "Decay", "%", 0.f, 100.f);
		configParam(BPM_PARAM, 60.f, 300.0f, 120.0f, "BPM", "", 0.f, 100.f);
		configParam(INFLUENCE_PARAM, 0.f, 1.f, 0.5f, "Influence Amount", "", 0.f, 100.f);
		configInput(AUDIO_INPUT, "Audio Input");
		configInput(CLOCK_INPUT, "Clock Input");
		configInput(RESET_INPUT, "Reset Input");
		configInput(INFLUENCE_INPUT, "Influence Input");
	}

	~Gator () {
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override {
	}

	int current_step = 0;
	float prev_audio_input = -2;
	float prev_clock_input = -2;
	float prev_reset_input = -2;
	int gate_ticks = 0;
	float gate_length = 0;
	int decay_ticks_length = 0;
	float clock_ticks = 0;
	float decay_delta = 0;
	float release_delta = 0;
	float gate_amplitude = 0;

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
		float bpmParam = params[BPM_PARAM].getValue();
		float decayParamIn = params[DECAY_PARAM].getValue();
		float decayParam = fmap_exp(decayParamIn, 0.0f, 1.0f);
		float influenceParam = params[INFLUENCE_PARAM].getValue();

		float audio_input = inputs[AUDIO_INPUT].getVoltage(0);
		// 0 or 10
		float clock_input = inputs[CLOCK_INPUT].getVoltage(0);
		float influence_input = inputs[INFLUENCE_INPUT].getVoltage(0);
		float reset_input = inputs[RESET_INPUT].getVoltage(0);

		bool have_clock = false;
		if (prev_clock_input < 1 && clock_input > 1) {
			// 44.1 = 1ms
			// 44.1 * 800 = 800 ms
			// 0% 97 mis
			// 50% should be at 30% decay after 120 ms
			// 50% should be at 65% decay after 120 ms
			// 75% should be at 40% decay after 120 ms
			// max decay length before linear release
			double ms = args.sampleRate * 0.001f;
			double div_decay = LERP(ms * 100.0f, ms * 2000.0f, decayParam);
			double div_release = ms * 10.0f;

			decay_ticks_length = ms * 100.0f;
			decay_delta = 1.0f + LOG0001 / div_decay;
			release_delta = 1.0f + LOG0001 / div_release;
			gate_amplitude = 1.0f;
			have_clock = true;
			//if (current_step == 7) {
			if (influence_input * influenceParam > 5.0f) {
				gate_length = gate_ticks * 2.0f;
			}
			else {
				gate_length = gate_ticks * 0.5f;
			}
			gate_ticks = 0;
			current_step++;
			if (current_step > 16) {
				current_step = 0;
			}
		}
		//DEBUG("%f", clock_input);
		prev_clock_input = clock_input;

		//outputs[LEFT_OUTPUT].setVoltage(audio_input * (gate_ticks < gate_length));
		outputs[LEFT_OUTPUT].setVoltage(gate_amplitude * 5.0f);
		outputs[RIGHT_OUTPUT].setVoltage(audio_input * gate_amplitude);
		gate_ticks++;
		if (gate_ticks < decay_ticks_length) {
			gate_amplitude *= decay_delta;
		}
		else {
			gate_amplitude *= release_delta;
		}
		if (gate_amplitude < 0.0001f) {
			gate_amplitude = 0;
		}
	}
};

struct GatorWidget : ModuleWidget {
	Label* labelPitch;

	GatorWidget(Gator* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/tzfmlead5HP.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));

		float spacingX = box.size.x / (float)4.0f;
		int x0 = spacingX;
		int x1 = box.size.x - spacingX;

		float y = 72;
		addParam(createParamCentered<RoundBlackKnob>(Vec(x0, y), module, Gator::BPM_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(Vec(x1, y), module, Gator::DECAY_PARAM));

		y = 142;
		addInput(createInputCentered<PJ301MPort>(Vec(x0, y), module, Gator::RESET_INPUT));
		//addParam(createParamCentered<RoundBlackKnob>(Vec(x0, y), module, Gator::EVEN_DETUNE_PARAM));
		//addParam(createParamCentered<RoundBlackKnob>(Vec(x1, y), module, Gator::ODD_DETUNE_PARAM));

		y = 190;
		addInput(createInputCentered<PJ301MPort>(Vec(x0, y), module, Gator::INFLUENCE_INPUT));
		addParam(createParamCentered<RoundBlackKnob>(Vec(x1, y), module, Gator::INFLUENCE_PARAM));
		//addParam(createParamCentered<RoundBlackKnob>(Vec(x1, y), module, Gator::ODD_AMOUNT_PARAM)); // PW

		y = 238;
		addInput(createInputCentered<PJ301MPort>(Vec(x0, y), module, Gator::CLOCK_INPUT));

		y = 296;
		addInput(createInputCentered<PJ301MPort>(Vec(x0, y), module, Gator::AUDIO_INPUT));

		y = 344;
		addOutput(createOutputCentered<PJ301MPort>(Vec(x0, y), module, Gator::LEFT_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(x1, y), module, Gator::RIGHT_OUTPUT));

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
		Gator * module = dynamic_cast<Gator*>(this->module);
		if (module) {
			labelPitch->text = module->getRootNoteName();
		}
		*/
		ModuleWidget::step();
	}
};

Model* modelGator = createModel<Gator, GatorWidget>("gator");
