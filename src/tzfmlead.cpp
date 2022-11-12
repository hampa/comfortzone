#include "plugin.hpp"
#include "oscillator.h"
#include <math.h>

#define LERP(a, b, f) (a * (1.0f - f)) + (b * f)

struct Tzfmlead : Module {
	enum ParamIds {
		ROOT_NOTE_PARAM,
		FM_PARAM,
		MODULATOR_OCTAVE_PARAM,
		CARRIER_OCTAVE_PARAM,
		MODULATOR_WAVE_PARAM,
		CARRIER_WAVE_PARAM,
		STEREO_PARAM,
		NUM_PARAMS
	};

	enum InputIds {
		FM_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		MODULATOR_OUTPUT,
		CARRIER_OUTPUT,
		FM_LEFT_OUTPUT,
		FM_RIGHT_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		LIGHT0_LIGHT,
		LIGHT1_LIGHT,
		NUM_LIGHTS
	};

	Oscillator oscCarrier;
	Oscillator oscModulator;
	Oscillator oscFmCenter;
	Oscillator oscFmLeft;
	Oscillator oscFmRight;

	Tzfmlead () {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(ROOT_NOTE_PARAM, -12.f, 12.f, 0, "Root Note", " Midi");
		configParam(FM_PARAM, 0.f, 1.f, 0.5f, "FM Amount", "%", 0.f, 100.f);
		configParam(STEREO_PARAM, 0, 1.f, 0.f, "Stereo spread", "%", 0.f, 100.f);
		configParam(MODULATOR_OCTAVE_PARAM, -4.0f, 4.f, 0.f, "Modulator Octave");
		configParam(CARRIER_OCTAVE_PARAM, -4.0f, 4.f, 0.f, "Carrier octave");
		configParam(MODULATOR_WAVE_PARAM, 0.f, Oscillator::WAVE_LAST - 1, 0.f, "Modulator Wave");
		configParam(CARRIER_WAVE_PARAM, 0, Oscillator::WAVE_LAST - 1, 0.f, "Carrier Wave");
		configInput(FM_INPUT, "FM Amount");
	}

	~Tzfmlead () {
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
		oscModulator.Init(e.sampleRate);
		oscCarrier.Init(e.sampleRate);
		oscFmLeft.Init(e.sampleRate);
		oscFmCenter.Init(e.sampleRate);
		oscFmRight.Init(e.sampleRate);

		oscCarrier.SetWaveform(Oscillator::WAVE_SAW);
		oscModulator.SetWaveform(Oscillator::WAVE_SAW);
		oscFmLeft.SetWaveform(Oscillator::WAVE_SAW);
		oscFmCenter.SetWaveform(Oscillator::WAVE_SAW);
		oscFmRight.SetWaveform(Oscillator::WAVE_SAW);
	}

	inline float mtof(float m) {
		return powf(2, (m - 69.0f) / 12.0f) * 440.0f;
	}

	void process(const ProcessArgs& args) override {
		float rootNoteMidi = floorf((params[ROOT_NOTE_PARAM].getValue()));
		float stereoParam = params[STEREO_PARAM].getValue();
		float fmParam = params[FM_PARAM].getValue();
		int modulatorOctave = floorf(params[MODULATOR_OCTAVE_PARAM].getValue()) + 4;
		int carrierOctave = floorf(params[CARRIER_OCTAVE_PARAM].getValue());
		int modulatorWave = floorf(params[MODULATOR_WAVE_PARAM].getValue());
		int carrierWave = floorf(params[CARRIER_WAVE_PARAM].getValue());

		fmNote = floorf(rootNoteMidi);

		float fmInput01 = 1.0f;
		if (inputs[FM_INPUT].isConnected()) {
			fmInput01 = inputs[FM_INPUT].getVoltage(0) * 0.1f;
		}

		oscModulator.SetWaveform(modulatorWave);
		oscCarrier.SetWaveform(carrierWave);
		oscFmLeft.SetWaveform(carrierWave);
		oscFmCenter.SetWaveform(carrierWave);
		oscFmRight.SetWaveform(carrierWave);

		float pitch = carrierOctave + (rootNoteMidi / 12.0f);
		float modulatorFreq = mtof(modulatorOctave * 12.0f + rootNoteMidi);
		float carrierFreq = mtof(carrierOctave * 12.0f + rootNoteMidi);

		oscModulator.SetFreq(modulatorFreq);
		oscCarrier.SetFreq(carrierFreq);

		float fModulator = oscModulator.Process();
		float fCarrier = oscCarrier.Process();

		oscFmLeft.SetTZFM(pitch, fModulator, fmParam * fmInput01, -stereoParam * 5.0f);
		oscFmCenter.SetTZFM(pitch, fModulator, fmParam * fmInput01, 0.0f);
		oscFmRight.SetTZFM(pitch, fModulator, fmParam * fmInput01, stereoParam * 5.0f);

		float center = oscFmLeft.Process();
		float left = oscFmLeft.Process();
		float right = oscFmRight.Process();
		float leftMix = LERP(center, left, 0.5f);
		float rightMix = LERP(center, right, 0.5f);

		outputs[MODULATOR_OUTPUT].setVoltage(fModulator * 5.0f);
		outputs[CARRIER_OUTPUT].setVoltage(fCarrier * 5.0f);
		outputs[FM_LEFT_OUTPUT].setVoltage(leftMix * 5.0f);
		outputs[FM_RIGHT_OUTPUT].setVoltage(rightMix * 5.0f);
	}
};

struct TzfmleadWidget : ModuleWidget {
	Label* labelPitch;

	TzfmleadWidget(Tzfmlead* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/tzfmlead5HP.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));

		float spacingX = box.size.x / (float)4.0f;
		int x0 = spacingX;
		int x1 = box.size.x - spacingX;

		float y = 72;
		addParam(createParamCentered<RoundBlackKnob>(Vec(x0, y), module, Tzfmlead::ROOT_NOTE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(Vec(x1, y), module, Tzfmlead::STEREO_PARAM));

		y = 142;
		addParam(createParamCentered<RoundBlackKnob>(Vec(x0, y), module, Tzfmlead::FM_PARAM));
		addInput(createInputCentered<PJ301MPort>(Vec(x1, y), module, Tzfmlead::FM_INPUT));

		y = 190;
		addParam(createParamCentered<RoundBlackKnob>(Vec(x0, y), module, Tzfmlead::MODULATOR_WAVE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(Vec(x1, y), module, Tzfmlead::CARRIER_WAVE_PARAM));

		y = 238;
		addParam(createParamCentered<RoundBlackKnob>(Vec(x0, y), module, Tzfmlead::MODULATOR_OCTAVE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(Vec(x1, y), module, Tzfmlead::CARRIER_OCTAVE_PARAM));

		y = 296;
		addOutput(createOutputCentered<PJ301MPort>(Vec(x0, y), module, Tzfmlead::MODULATOR_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(x1, y), module, Tzfmlead::CARRIER_OUTPUT));

		y = 344;
		addOutput(createOutputCentered<PJ301MPort>(Vec(x0, y), module, Tzfmlead::FM_LEFT_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(x1, y), module, Tzfmlead::FM_RIGHT_OUTPUT));

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
		Tzfmlead * module = dynamic_cast<Tzfmlead*>(this->module);
		if (module) {
			labelPitch->text = module->getRootNoteName();
		}
		ModuleWidget::step();
	}
};

Model* modelTzfmlead = createModel<Tzfmlead, TzfmleadWidget>("tzfmlead");
