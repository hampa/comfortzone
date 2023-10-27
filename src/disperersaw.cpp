#include "plugin.hpp"
#include "oscillator.h"
#include <math.h>

#define LERP(a, b, f) (a * (1.0f - f)) + (b * f)
#define NUM_OSC 64

struct DisperserSaw : Module {
	enum ParamIds {
		PHASE_OFFSET_PARAM,
		PHASE_INC_PARAM,
		FM_FREQ_PARAM,
		FM_AMOUNT_PARAM,
		MULT_PARAM,
		DISPERSE_PARAM,
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

	DisperserSaw () {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(PHASE_OFFSET_PARAM, 0.f, 1.f, 0.5f, "Phase Offset", "%", 0.f, 100.f);
		configParam(DISPERSE_PARAM, 0.f, 1.f, 0.5f, "Disperse", "%", 0.f, 100.f);
		configParam(MULT_PARAM, 0.f, 1.f, 0.5f, "Mult", "%", 0.f, 100.f);
		configParam(PHASE_INC_PARAM, 0.f, 1.f, 0.5f, "Phase Inc", "%", 0.f, 100.f);
		configParam(FM_FREQ_PARAM, 0.f, 1.f, 0.5f, "FM Freq", "%", 0.f, 100.f);
		configParam(FM_AMOUNT_PARAM, 0.f, 1.f, 0.5f, "FM Amount", "%", 0.f, 100.f);
		configInput(PHASER_WIDTH_INPUT, "Phaser Width FM");
		configInput(PHASER_OFFSET_INPUT, "Phaser Offset FM");
	}

	~DisperserSaw () {
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
	float samplerateInv = 0.0f;
	float phase01 = 0.0f;

	void onSampleRateChange(const SampleRateChangeEvent& e) override {
		for (int i = 0; i < NUM_OSC; i++) {
			osc[i].Init(e.sampleRate);
			osc[i].SetWaveform(Oscillator::WAVE_SIN);
			partials[i] = 0;
		}
		oscFm.Init(e.sampleRate);
		oscFm.SetWaveform(Oscillator::WAVE_POLYBLEP_SQUARE);
		samplerateInv = 1.0f / (float)e.sampleRate;
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

	float getOvertone(int x, float _phase, float offset) {
		float amp_weight = 1.0f / (float)x;
		float overtone = amp_weight * sinf(2.0f * M_PI * _phase * x + offset);
		return overtone;
	}

	float kbEucMod(float a, float b) {
		float mod = fmod(a, b);
		if (mod < 0.f) {
			mod += b;
		}
		return mod;
	}

	float getSigmoid(float x, float k) {
                x = clamp(x, 0.0f, 1.0f);
                k = clamp(k, -1.0f, 1.0f);
                float a = k - x * 2.0f * k + 1.0f;
                float b = x - x * k;
                return (b / a);
        }

	bool isDirty = false;

	#define NUM_PARTIALS 256// number of partials to use
	#define SAMPLE_RATE 44100  // audio sample rate
	#define TWO_PI 6.28318530718f  // constant for 2*pi

	void createSaw(float *buffer, int numSamples, float frequency, float dispersion) {
		float phaseIncrement = frequency / SAMPLE_RATE * TWO_PI;
		float weight = 1;
		for (int i = 0; i < numSamples; i++) {
			float phase = i * phaseIncrement;
			float value = 0;
			for (int j = 1; j <= NUM_PARTIALS; j++) {
				value += weight * sinf(phase * j);
				weight *= expf(-dispersion * j);
			}
			buffer[i] = value;
		}
	}


	float additiveSaw(float phase, float dispersion) {
		float value = 0;
		float weight = 1;
		for (int i = 1; i <= NUM_PARTIALS; i++) {
			value += weight * sinf(phase * i);
			weight *= expf(-dispersion * i);
		}
		return value;
	}

	float expVal(float input) {
		float exp_min = log(1);
		float exp_max = log(100);
		float exp_range = exp_max - exp_min;
		return (exp(input * exp_range + exp_min));
	}

	float overtoneSaw(float phase, float dispersion, float offset, float divs, float mult, float phaseOffset) {
		float out = 0;
		float delta = 1.0f / (float) NUM_PARTIALS;
		float t = offset;
		for (int i = 1; i <= NUM_PARTIALS; i++) {
			//float tt = expVal(t) * 0.01f;
			float disp = LERP(-M_PI, M_PI, t) * dispersion;
			out += getOvertone(i, phase + phaseOffset, disp);
			t += delta * (1.0f + divs);
			t = kbEucMod(t, 1.0f);
			delta *= mult;
		}
		return out;
		//return out / (float)NUM_PARTIALS;
	}

	int numSamples = 0;
	// serum wavetable
	// p = 0 - 1
	// bin 0.0 = sin(x - p * PI_2)
	// bin 0.5 = sin(x + PI)
	void process(const ProcessArgs& args) override {
		float phaseOffsetParam = params[PHASE_OFFSET_PARAM].getValue();
		float disperseParam = params[DISPERSE_PARAM].getValue();
		float phaseIncParam = params[PHASE_INC_PARAM].getValue();
		float fmMidi = params[FM_FREQ_PARAM].getValue() * 127.0f - 64.0f;
		float fmFreq = mtof(fmMidi);
		float delta = fmFreq * samplerateInv;
		float dispersionAmount = phaseIncParam;
		float dispersionDivs = disperseParam * 128.0f;
		float mult = LERP(0.8f, 1.2f, params[MULT_PARAM].getValue());
		float phaseOffset = phaseOffsetParam;
		float dispersionOffset = params[FM_FREQ_PARAM].getValue();

		//float phaseIncrement = frequency / SAMPLE_RATE * TWO_PI;
		//float phaseIncrement = frequency / SAMPLE_RATE * TWO_PI;
		float phase = 1.0f - (numSamples / 1024.0f);
		//float out = additiveSaw(phase, dispersion);
		float out = overtoneSaw(phase, dispersionAmount * 2.0f, dispersionOffset, dispersionDivs, mult, phaseOffset);

		//DEBUG("%i %f %f", numSamples, phase, out);
		numSamples++;
		if (numSamples >= 1024) {
			numSamples = 0;
		}

		outputs[FM_LEFT_OUTPUT].setVoltage(5.0f * (numSamples < 100));
		outputs[FM_RIGHT_OUTPUT].setVoltage(out * 2.5f);
	}
};

struct DisperserSawWidget : ModuleWidget {
	Label* labelPitch;

	DisperserSawWidget(DisperserSaw* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/tzfmlead5HP.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));

		float spacingX = box.size.x / (float)4.0f;
		int x0 = spacingX;
		int x1 = box.size.x - spacingX;

		float y = 72;
		addParam(createParamCentered<RoundBlackKnob>(Vec(x0, y), module, DisperserSaw::PHASE_OFFSET_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(Vec(x1, y), module, DisperserSaw::PHASE_INC_PARAM));
		//addInput(createInputCentered<PJ301MPort>(Vec(x1, y), module, DisperserSaw::PHASER_WIDTH_INPUT));

		y = 142;
		//addParam(createParamCentered<RoundBlackKnob>(Vec(x0, y), module, DisperserSaw::PHASE_OFFSET_PARAM));
		//addInput(createInputCentered<PJ301MPort>(Vec(x1, y), module, DisperserSaw::PHASER_OFFSET_INPUT));

		y = 190;
		addParam(createParamCentered<RoundBlackKnob>(Vec(x0, y), module, DisperserSaw::FM_FREQ_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(Vec(x1, y), module, DisperserSaw::FM_AMOUNT_PARAM));

		y = 238;
		addParam(createParamCentered<RoundBlackKnob>(Vec(x0, y), module, DisperserSaw::DISPERSE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(Vec(x1, y), module, DisperserSaw::MULT_PARAM));

		y = 296;
		//addOutput(createOutputCentered<PJ301MPort>(Vec(x0, y), module, DisperserSaw::MODULATOR_OUTPUT));
		//addOutput(createOutputCentered<PJ301MPort>(Vec(x1, y), module, DisperserSaw::CARRIER_OUTPUT));

		y = 344;
		addOutput(createOutputCentered<PJ301MPort>(Vec(x0, y), module, DisperserSaw::FM_LEFT_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(x1, y), module, DisperserSaw::FM_RIGHT_OUTPUT));

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
		DisperserSaw * module = dynamic_cast<DisperserSaw*>(this->module);
		if (module) {
			labelPitch->text = module->getRootNoteName();
		}
		ModuleWidget::step();
	}
};

Model* modelDisperserSaw = createModel<DisperserSaw, DisperserSawWidget>("dispersersaw");
