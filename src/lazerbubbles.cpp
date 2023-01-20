#include "plugin.hpp"
#include "oscillator.h"
#include <math.h>

#define LERP(a, b, f) (a * (1.0f - f)) + (b * f)

struct LazerBubbles : Module {
	enum ParamIds {
		FREQ_PARAM,
		AMOUNT_PARAM,
		WARP_PARAM,
		WAVE_PARAM,
		NUM_PARAMS
	};

	enum InputIds {
		PHASER_WIDTH_INPUT,
		PHASER_OFFSET_INPUT,
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

	enum {
		WAVE_SIN,
		WAVE_SAW,
		WAVE_SQUARE,
		NUM_WAVES
	};

	LazerBubbles () {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(FREQ_PARAM, 0.f, 1.f, 0.5f, "Freq", "%", 0.f, 100.f);
		configParam(AMOUNT_PARAM, 0.f, 1.f, 0.5f, "Amount");
		configParam(WARP_PARAM, 0.f, 1.0f, 0.5f, "Warp");
		configParam(WAVE_PARAM, 0.f, NUM_WAVES - 1, 0.0f, "Wave");
		configInput(PHASER_WIDTH_INPUT, "Phaser Width FM");
		configInput(PHASER_OFFSET_INPUT, "Phaser Offset FM");
	}

	~LazerBubbles () {
	}

	const char *getNoteName(int i) {
		const char *notes[] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "G#", "A", "Bb", "B" };
		return notes[(i + 12) % 12];
	}
	Oscillator oscRoot;
	Oscillator oscGrain;

	int fmNote = 0;
	const char *getRootNoteName() {
		return getNoteName(fmNote);
	}

	//float samplerateInv = 0.0f;
	//float phase01 = 0.0f;

	void onSampleRateChange(const SampleRateChangeEvent& e) override {
		//samplerateInv = 1.0f / (float)e.sampleRate;
		oscRoot.Init(e.sampleRate);
		oscRoot.SetWaveform(Oscillator::WAVE_SIN);
		oscGrain.Init(e.sampleRate);
		oscGrain.SetWaveform(Oscillator::WAVE_SIN);
	}

	inline float mtof(float m) {
		return powf(2, (m - 69.0f) / 12.0f) * 440.0f;
	}

	/*
	float getSaw(float phase, float deltaPhase) {
		float sawBuffer[3];
		float phases[3];

		phases[0] = phase - 2 * deltaPhase + (phase < 2 * deltaPhase);
		phases[1] = phase - deltaPhase + (phase < deltaPhase);
		phases[2] = phase;

		const float denominator = 0.25f / (deltaPhase * deltaPhase);

		for (int i = 0; i < 3; ++i) {
			float p = 2 * phases[i] - 1.0f;
			sawBuffer[i] = (p * p * p - p) / 6.0;
		}
		return (sawBuffer[0] - 2.0 * sawBuffer[1] + sawBuffer[2]) * denominator;
	}
	*/

	// 0=left 0.5=nothing 1=right
	float bendPhase(float phase, float amount) {
		float offset = (amount - amount * amount) * 2.0f;
		float phase2 = phase * phase;
		float phase3 = phase * phase2;
		float scale = amount * 3.0f;
		float middle_mult1 = scale + offset;
		float middle_mult2 = scale - offset;
		float middle1 = middle_mult1 * (phase2 - phase3);
		float middle2 = middle_mult2 * (phase - phase2 * 2.0f + phase3);
		float new_phase = phase3 + middle1 + middle2;
		return new_phase;
	}

	float kbEucMod(float a, float b) {
		float mod = fmod(a, b);
		if (mod < 0.f) {
			mod += b;
		}
		return mod;
	}

	float prevAmPhase;
	float prevSquare = 0;
	int periodIdx = 0;
	int rootIdx = 0;
	void process(const ProcessArgs& args) override {
		float freqMidi = params[FREQ_PARAM].getValue() * 127.0f - 64.0f;
		float amount = params[AMOUNT_PARAM].getValue();
		float warp = params[WARP_PARAM].getValue();
		int wave = floorf(params[WAVE_PARAM].getValue());
		float freq = mtof(freqMidi);

		oscRoot.SetFreq(freq);
		float outL = oscRoot.Process();
		float amPhase = oscRoot.GetPhase();
		if (amPhase < prevAmPhase) {
			oscGrain.Reset();
			rootIdx++;
			periodIdx = (rootIdx % 2);
		}
		prevAmPhase = amPhase;

		float phase01 = oscRoot.GetPhase01();
		float distortedPhase = bendPhase(phase01, warp);
		float grainFreq = LERP(freq, 18000, amount);
		oscGrain.SetFreq(grainFreq);

		//DEBUG("%f", phase01);
		//oscGrain.AddPhase(distortedPhase);
		//float outR = oscGrain.ProcessOffset(distortedPhase);
		//float delta = freq * samplerateInv;

		float distPhaseScaled = distortedPhase * (1.0f + amount * 50.0f);
		float sinus = sinf(distPhaseScaled * M_PI);
		float saw01 = kbEucMod(distPhaseScaled, 1.0f);
		float saw = -1.0f + saw01 * 2.0f;
		float square = saw01 < 0.5f ? (1.0f) : -1.0f;
		if (square < 0 && prevSquare > 0) {
			periodIdx++;
		}
		float outR = 0;
		prevSquare = square;
		/*
		//if ((periodIdx % 2) == 0) {
		if ((rootIdx % 2) == 0) {
			outR = square;
		}
		else {
			outR = sinus;
		}
		*/
		switch (wave) {
		case WAVE_SIN:
			outR = sinus;
			break;
		case WAVE_SQUARE:
			outR = square;
			break;
		case WAVE_SAW:
			outR = saw;
			break;
		default:
			outR = sinus;
			break;
		}
		//float outL = getSaw(phase01, delta);
		//phase01 += delta;
		//phase01 = kbEucMod(phase01, 1.0f);

		outputs[LEFT_OUTPUT].setVoltage(outL * 5.0f);
		outputs[RIGHT_OUTPUT].setVoltage(outR * 5.0f);
	}
};

struct LazerBubblesWidget : ModuleWidget {
	Label* labelPitch;

	LazerBubblesWidget(LazerBubbles* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/tzfmlead5HP.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));

		float spacingX = box.size.x / (float)4.0f;
		int x0 = spacingX;
		int x1 = box.size.x - spacingX;

		float y = 72;
		addParam(createParamCentered<RoundBlackKnob>(Vec(x0, y), module, LazerBubbles::FREQ_PARAM));
		addInput(createInputCentered<PJ301MPort>(Vec(x1, y), module, LazerBubbles::PHASER_WIDTH_INPUT));

		y = 142;
		//addParam(createParamCentered<RoundBlackKnob>(Vec(x0, y), module, LazerBubbles::EVEN_DETUNE_PARAM));
		//addParam(createParamCentered<RoundBlackKnob>(Vec(x1, y), module, LazerBubbles::ODD_DETUNE_PARAM));

		y = 190;
		addParam(createParamCentered<RoundBlackKnob>(Vec(x0, y), module, LazerBubbles::AMOUNT_PARAM)); // PW
		addParam(createParamCentered<RoundBlackKnob>(Vec(x1, y), module, LazerBubbles::WARP_PARAM)); // PW

		y = 238;
		addParam(createParamCentered<RoundBlackKnob>(Vec(x0, y), module, LazerBubbles::WAVE_PARAM));
		//addParam(createParamCentered<RoundBlackKnob>(Vec(x1, y), module, LazerBubbles::AM_WAVE_PARAM));

		y = 296;
		//addOutput(createOutputCentered<PJ301MPort>(Vec(x0, y), module, LazerBubbles::MODULATOR_OUTPUT));
		//addOutput(createOutputCentered<PJ301MPort>(Vec(x1, y), module, LazerBubbles::CARRIER_OUTPUT));

		y = 344;
		addOutput(createOutputCentered<PJ301MPort>(Vec(x0, y), module, LazerBubbles::LEFT_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(x1, y), module, LazerBubbles::RIGHT_OUTPUT));

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
		LazerBubbles * module = dynamic_cast<LazerBubbles*>(this->module);
		if (module) {
			labelPitch->text = module->getRootNoteName();
		}
		ModuleWidget::step();
	}
};

Model* modelLazerBubbles = createModel<LazerBubbles, LazerBubblesWidget>("lazerbubbles");
