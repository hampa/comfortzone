#include "plugin.hpp"
#include "oscillator.h"
#include <math.h>

#define LERP(a, b, f) (a * (1.0f - f)) + (b * f)
#define NUM_OSC 76

struct Sundial : Module {
	enum ParamIds {
		PHASER_WIDTH_PARAM,
		PHASER_OFFSET_PARAM,
		FM_FREQ_PARAM,
		FM_AMOUNT_PARAM,
		WAVETABLE_SCAN_PARAM,
		NUM_PARAMS
	};

	enum InputIds {
		PHASER_WIDTH_INPUT,
		PHASER_OFFSET_INPUT,
		NUM_INPUTS
	};

	enum OutputIds {
		FM_LEFT_OUTPUT,
		FM_RIGHT_OUTPUT,
		NUM_OUTPUTS
	};

	enum LightIds {
		LIGHT0_LIGHT,
		LIGHT1_LIGHT,
		NUM_LIGHTS
	};

	Oscillator osc[NUM_OSC];
	Oscillator oscFm;
	float partials[NUM_OSC];

	Sundial () {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(PHASER_WIDTH_PARAM, 0.f, 1.f, 0.5f, "Phaser Width", "%", 0.f, 100.f);
		configParam(PHASER_OFFSET_PARAM, 0.f, 1.f, 0.5f, "Phaser Offset", "%", 0.f, 100.f);
		configParam(WAVETABLE_SCAN_PARAM, 0.f, 1.f, 0.5f, "Wavetable Scan", "%", 0.f, 100.f);
		configParam(FM_FREQ_PARAM, 0.f, 1.f, 0.5f, "FM Freq", "%", 0.f, 100.f);
		configParam(FM_AMOUNT_PARAM, 0.f, 1.f, 0.5f, "FM Amount", "%", 0.f, 100.f);
		configInput(PHASER_WIDTH_INPUT, "Phaser Width FM");
		configInput(PHASER_OFFSET_INPUT, "Phaser Offset FM");
	}

	~Sundial () {
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

	int wavetable_idx = 0;
#define NUM_WAVETABLES 16
	float wavetableProcess[33];
	Oscillator wtOsc[NUM_WAVETABLES][3];
	int wavetables[NUM_WAVETABLES][3] = {
		{1, 5,  8}, // 0
		{1, 7, 10}, // 1
		{1, 7, 16}, // 2
		{1,11, 19}, // 3
		{1,14, 18}, // 4
		{1,13, 18}, // 5
		{1,17, 22}, // 6
		{1,16, 24}, // 7
		{1, 7, 18}, // 8
		{1,10, 18}, // 9
		{1,18, 21}, // 10
		{1,12, 22}, // 11
		{1,19, 25}, // 12
		{1, 8, 21}, // 13
		{1, 8, 30}, // 14
		{1,21, 32}  // 15
	};

	void onSampleRateChange(const SampleRateChangeEvent& e) override {
		for (int i = 0; i < NUM_OSC; i++) {
			osc[i].Init(e.sampleRate);
			osc[i].SetWaveform(Oscillator::WAVE_SIN);
			partials[i] = 0;
		}
		for (int i = 0; i < NUM_WAVETABLES; i++) {
			for (int x = 0; x < 3; x++) {
				wtOsc[i][x].Init(e.sampleRate);
				wtOsc[i][x].SetWaveform(Oscillator::WAVE_SIN);
			}
		}
		oscFm.Init(e.sampleRate);
		oscFm.SetWaveform(Oscillator::WAVE_POLYBLEP_SQUARE);
	}

	inline float mtof(float m) {
		return powf(2, (m - 69.0f) / 12.0f) * 440.0f;
	}

	/*
	inline int getOffset(float f) {
		int idx = floorf(f) % NUM_OSC;
	}
	*/

	inline float getPartial(float f) {
		int idxA = floorf(f);
		idxA %= NUM_OSC;
		int idxB = idxA + 1;
		float rem = f - floorf(f);
		if (idxA < 0) {
			idxA = NUM_OSC - 1;
			idxB = 0;
		}
		if (idxB > NUM_OSC) {
			idxB = 0;
		}
		float result = LERP(partials[idxA], partials[idxB], rem);
		return result;
	}

	void process(const ProcessArgs& args) override {
		float phaserWidth = params[PHASER_WIDTH_PARAM].getValue();
		float phaserOffset = params[PHASER_OFFSET_PARAM].getValue();
		float wavetableScan = params[WAVETABLE_SCAN_PARAM].getValue();
		float fmAmount = params[FM_AMOUNT_PARAM].getValue();
		float fmMidi = params[FM_FREQ_PARAM].getValue() * 127.0f - 64.0f;
		float fmFreq = mtof(fmMidi);

		float phaserWidthInput = 1.0f;
		if (inputs[PHASER_WIDTH_INPUT].isConnected()) {
			phaserWidthInput = fabs(inputs[PHASER_WIDTH_INPUT].getVoltage());
		}

		float phaserOffsetInput = 1.0f;
		if (inputs[PHASER_OFFSET_INPUT].isConnected()) {
			phaserOffsetInput = fabs(inputs[PHASER_OFFSET_INPUT].getVoltage());
		}

		float width = phaserWidthInput * (phaserWidth * NUM_OSC) + 3;
		float offset = phaserOffsetInput * phaserOffset * (NUM_OSC - 1);
		offset = floorf(offset);

		float fCV = 0;
		float inc = 1.0f/8.0f;

		oscFm.SetFreq(fmFreq);
		float fm = oscFm.Process() * fmAmount;

		/*
		for (int i = 0; i < 33; i++) {
			osc[i].SetFreq(fmFreq * (i + 1.0f));
			wavetableProcess[i] = osc[i].Process();
		}
		*/
		for (int i = 0; i < NUM_WAVETABLES; i++) {
			for (int x = 0; x < 3; x++) {
				int p = wavetables[i][x];
				//float freq = powf(2.0f, fCV + fm) * (32.703251f * 0.5f);
				wtOsc[i][x].SetFreq((fmFreq * p) * (1.0f + fm));
				//wtOsc[i][x].SetFreq(fmFreq * (i + 1.0f));
			}
		}

		float wavetableVal = LERP(0, (NUM_WAVETABLES - 1), wavetableScan);
		int wavetableIdx = floorf(wavetableVal);
		float wavetableLerp = wavetableVal - wavetableIdx;
		int nextIdx = (wavetableIdx == NUM_WAVETABLES - 1) ? 0 : wavetableIdx + 1;
		float f0 = 0;
		for (int i = 0; i < 3; i++) {
			//f0 += wavetableProcess[wavetables[wavetableIdx][i]];
			f0 += wtOsc[wavetableIdx][i].Process();
		}
		float f1 = 0;
		for (int i = 0; i < 3; i++) {
			//f1 += wavetableProcess[wavetables[nextIdx][i]];
			f1 += wtOsc[nextIdx][i].Process();
		}
		//float fOut = LERP(f0, f1, wavetableLerp);
		float fOut = f0;
		//float f0 = getPartial(
		/*
		for (int i = 0; i < NUM_OSC; i++) {
			float freq = powf(2.0f, fCV + fm) * (32.703251f * 0.5f);
			osc[i].SetFreq(freq);
			partials[i] = osc[i].Process();
			fCV += inc;
		}
		*/

		/*
		float f0 = getPartial(offset - 1) * 0.15f;
		float f1 = getPartial(offset) * 0.7;
		float f2 = getPartial(offset + 1) * 0.15f;
		float fOut = f0 + f1 + f2;
		*/

		/*
		float fOut = 0;
		float count = 0;
		for (float f = 0; f < NUM_OSC; f += width) {
			float f0 = getPartial(f + offset + width - 1) * 0.15f;
			float f1 = getPartial(f + offset + width) * 0.7;
			float f2 = getPartial(f + offset + width + 1) * 0.15f;
			fOut += f0 + f1 + f2;
			count += 1.0f;
		}
		fOut /= count;
		*/

		//fOut /= 1.0f;
		//fOut /= (float)i;
		//fOut /= (float)NUM_OSC;

		outputs[FM_LEFT_OUTPUT].setVoltage(fOut * 5.0f);
		outputs[FM_RIGHT_OUTPUT].setVoltage(fOut * 5.0f);
	}
};

struct SundialWidget : ModuleWidget {
	Label* labelPitch;

	SundialWidget(Sundial* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/tzfmlead5HP.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));

		float spacingX = box.size.x / (float)4.0f;
		int x0 = spacingX;
		int x1 = box.size.x - spacingX;

		float y = 72;
		addParam(createParamCentered<RoundBlackKnob>(Vec(x0, y), module, Sundial::PHASER_WIDTH_PARAM));
		addInput(createInputCentered<PJ301MPort>(Vec(x1, y), module, Sundial::PHASER_WIDTH_INPUT));

		y = 142;
		addParam(createParamCentered<RoundBlackKnob>(Vec(x0, y), module, Sundial::PHASER_OFFSET_PARAM));
		addInput(createInputCentered<PJ301MPort>(Vec(x1, y), module, Sundial::PHASER_OFFSET_INPUT));

		y = 190;
		addParam(createParamCentered<RoundBlackKnob>(Vec(x0, y), module, Sundial::FM_FREQ_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(Vec(x1, y), module, Sundial::FM_AMOUNT_PARAM));

		y = 238;
		addParam(createParamCentered<RoundBlackKnob>(Vec(x0, y), module, Sundial::WAVETABLE_SCAN_PARAM));
		//addParam(createParamCentered<RoundBlackKnob>(Vec(x1, y), module, Sundial::CARRIER_OCTAVE_PARAM));

		y = 296;
		//addOutput(createOutputCentered<PJ301MPort>(Vec(x0, y), module, Sundial::MODULATOR_OUTPUT));
		//addOutput(createOutputCentered<PJ301MPort>(Vec(x1, y), module, Sundial::CARRIER_OUTPUT));

		y = 344;
		addOutput(createOutputCentered<PJ301MPort>(Vec(x0, y), module, Sundial::FM_LEFT_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(x1, y), module, Sundial::FM_RIGHT_OUTPUT));

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
		Sundial * module = dynamic_cast<Sundial*>(this->module);
		if (module) {
			labelPitch->text = module->getRootNoteName();
		}
		ModuleWidget::step();
	}
};

Model* modelSundial = createModel<Sundial, SundialWidget>("sundial");
