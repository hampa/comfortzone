#include "plugin.hpp"
#include "oscillator.h"
#include "am.h"
#include <math.h>

#define LERP(a, b, f) (a * (1.0f - f)) + (b * f)
#define NUM_OSC 76

struct Graintable : Module {
	enum ParamIds {
		FREQ_PARAM,
		ODD_DETUNE_PARAM,
		EVEN_DETUNE_PARAM,
		EVEN_AMOUNT_PARAM,
		ODD_AMOUNT_PARAM,
		GRAIN_WAVE_PARAM,
		AM_WAVE_PARAM,
		NUM_PARAMS
	};

	enum InputIds {
		PHASER_WIDTH_INPUT,
		PHASER_OFFSET_INPUT,
		NUM_INPUTS
	};

	enum OutputIds {
		EVEN_ODD_OUTPUT,
		LEFT_OUTPUT,
		RIGHT_OUTPUT,
		NUM_OUTPUTS
	};

	enum LightIds {
		LIGHT0_LIGHT,
		LIGHT1_LIGHT,
		NUM_LIGHTS
	};

	Oscillator oscGrain;
	AM amWave;

	Graintable () {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(FREQ_PARAM, 0.f, 1.f, 0.5f, "Freq", "%", 0.f, 100.f);
		configParam(ODD_DETUNE_PARAM, -4.f, 4.f, 0.0f, "Odd Detune");
		configParam(EVEN_DETUNE_PARAM, -4.f, 4.f, 0.0f, "Even Detune");
		configParam(EVEN_AMOUNT_PARAM, 0.f, 1.f, 0.5f, "Even Amount", "%", 0.f, 100.f);
		configParam(ODD_AMOUNT_PARAM, 0.f, 1.f, 0.5f, "Odd Amount", "%", 0.f, 100.f);
		configParam(GRAIN_WAVE_PARAM, 0.f, Oscillator::WAVE_LAST - 1, 0.0f, "Grain Wave");
		configParam(AM_WAVE_PARAM, 0.f, AM::WAVE_LAST - 1, 0.5f, "AM Wave");
		configInput(PHASER_WIDTH_INPUT, "Phaser Width FM");
		configInput(PHASER_OFFSET_INPUT, "Phaser Offset FM");
	}

	~Graintable () {
	}

	const char *getNoteName(int i) {
		const char *notes[] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "G#", "A", "Bb", "B" };
		return notes[(i + 12) % 12];
	}

	int fmNote = 0;
	const char *getRootNoteName() {
		return getNoteName(fmNote);
	}

	float phaseModulator = 0;
	float phaseCarrier = 0;
	float prev_center = 0;
	float prev_left = 0;

	void onSampleRateChange(const SampleRateChangeEvent& e) override {
		oscGrain.Init(e.sampleRate);
		oscGrain.SetWaveform(Oscillator::WAVE_SIN);
		amWave.Init(e.sampleRate);
		amWave.SetWaveform(AM::WAVE_TRI);
	}

	inline float mtof(float m) {
		return powf(2, (m - 69.0f) / 12.0f) * 440.0f;
	}

	/*
	inline int getOffset(float f) {
		int idx = floorf(f) % NUM_OSC;
	}
	*/

	float prevAmPhase = -1;
	int grainIdx = 0;
	void process(const ProcessArgs& args) override {
		float freqMidi = params[FREQ_PARAM].getValue() * 127.0f - 64.0f;
		float evenAmountParam = params[EVEN_AMOUNT_PARAM].getValue();
		float oddAmountParam = params[ODD_AMOUNT_PARAM].getValue();
		int amWaveform = floorf(params[AM_WAVE_PARAM].getValue());
		int grainWaveform = floorf(params[GRAIN_WAVE_PARAM].getValue());
		int oddDetune = floorf(params[ODD_DETUNE_PARAM].getValue());
		int evenDetune = floorf(params[EVEN_DETUNE_PARAM].getValue());

		int oddMidi = freqMidi - oddDetune * 12;
		int evenMidi = freqMidi + evenDetune * 12;
		float freq = mtof(freqMidi);
		float oddFreq = mtof(oddMidi);
		float evenFreq = mtof(evenMidi);

		amWave.SetWaveform(amWaveform);
		oscGrain.SetWaveform(grainWaveform);

		amWave.SetFreq(freq);
		float window = amWave.Process();
		float amPhase = amWave.GetPhase();
		if (amPhase < prevAmPhase) {
			oscGrain.Reset();
			grainIdx++;
		}
		prevAmPhase = amPhase;

		float grainFreq = oddFreq;
		float amountParam = oddAmountParam;
		if ((grainIdx % 2) == 0) {
			grainFreq = evenFreq;
			amountParam = evenAmountParam;
		}

		oscGrain.SetFreq(grainFreq * ((amountParam  * 32.0f) + 1.0f));
		float fOut = oscGrain.Process() * window;

		outputs[LEFT_OUTPUT].setVoltage(window * 5.0f);
		outputs[RIGHT_OUTPUT].setVoltage(fOut * 5.0f);
		outputs[EVEN_ODD_OUTPUT].setVoltage(amountParam);
	}
};

struct GraintableWidget : ModuleWidget {
	Label* labelPitch;

	GraintableWidget(Graintable* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/tzfmlead5HP.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));

		float spacingX = box.size.x / (float)4.0f;
		int x0 = spacingX;
		int x1 = box.size.x - spacingX;

		float y = 72;
		addParam(createParamCentered<RoundBlackKnob>(Vec(x0, y), module, Graintable::FREQ_PARAM));
		addInput(createInputCentered<PJ301MPort>(Vec(x1, y), module, Graintable::PHASER_WIDTH_INPUT));

		y = 142;
		addParam(createParamCentered<RoundBlackKnob>(Vec(x0, y), module, Graintable::EVEN_DETUNE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(Vec(x1, y), module, Graintable::ODD_DETUNE_PARAM));

		y = 190;
		addParam(createParamCentered<RoundBlackKnob>(Vec(x0, y), module, Graintable::EVEN_AMOUNT_PARAM)); // PW
		addParam(createParamCentered<RoundBlackKnob>(Vec(x1, y), module, Graintable::ODD_AMOUNT_PARAM)); // PW

		y = 238;
		addParam(createParamCentered<RoundBlackKnob>(Vec(x0, y), module, Graintable::GRAIN_WAVE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(Vec(x1, y), module, Graintable::AM_WAVE_PARAM));

		y = 296;
		addOutput(createOutputCentered<PJ301MPort>(Vec(x0, y), module, Graintable::EVEN_ODD_OUTPUT));
		//addOutput(createOutputCentered<PJ301MPort>(Vec(x0, y), module, Graintable::MODULATOR_OUTPUT));
		//addOutput(createOutputCentered<PJ301MPort>(Vec(x1, y), module, Graintable::CARRIER_OUTPUT));

		y = 344;
		addOutput(createOutputCentered<PJ301MPort>(Vec(x0, y), module, Graintable::LEFT_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(x1, y), module, Graintable::RIGHT_OUTPUT));

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
		Graintable * module = dynamic_cast<Graintable*>(this->module);
		if (module) {
			labelPitch->text = module->getRootNoteName();
		}
		ModuleWidget::step();
	}
};

Model* modelGraintable = createModel<Graintable, GraintableWidget>("graintable");
