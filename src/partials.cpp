#include "plugin.hpp"
#include "oscillator.h"
#include <math.h>

#define LERP(a, b, f) (a * (1.0f - f)) + (b * f)
#define NUM_OSC 64

struct Partials : Module {
	enum ParamIds {
		PHASE_OFFSET_PARAM,
		PHASE_INC_PARAM,
		FM_FREQ_PARAM,
		FM_AMOUNT_PARAM,
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

	Partials () {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(PHASE_OFFSET_PARAM, 0.f, 1.f, 0.5f, "Phase Offset", "%", 0.f, 100.f);
		configParam(PHASE_INC_PARAM, 0.f, 1.f, 0.5f, "Phase Inc", "%", 0.f, 100.f);
		configParam(FM_FREQ_PARAM, 0.f, 1.f, 0.5f, "FM Freq", "%", 0.f, 100.f);
		configParam(FM_AMOUNT_PARAM, 0.f, 1.f, 0.5f, "FM Amount", "%", 0.f, 100.f);
		configInput(PHASER_WIDTH_INPUT, "Phaser Width FM");
		configInput(PHASER_OFFSET_INPUT, "Phaser Offset FM");
	}

	~Partials () {
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
	int buffer_idx = 0;
	float buffer[4096];

	// serum wavetable
	// p = 0 - 1
	// bin 0.0 = sin(x - p * PI_2)
	// bin 0.5 = sin(x + PI)
	void process(const ProcessArgs& args) override {
		float phaseOffsetParam = params[PHASE_OFFSET_PARAM].getValue();
		float phaseIncParam = params[PHASE_INC_PARAM].getValue();
		float fmAmount = params[FM_AMOUNT_PARAM].getValue();
		float fmMidi = params[FM_FREQ_PARAM].getValue() * 127.0f - 64.0f;
		float fmFreq = mtof(fmMidi);
		float delta = fmFreq * samplerateInv;

		float fCV = 0;
		float inc = 1.0f/8.0f;

		oscFm.SetFreq(fmFreq);
		float fm = oscFm.Process() * fmAmount;

		float fOut = 0;
		float phaseInc = phaseOffsetParam;
		float offset = 0;
		int partials_start = 0;
		int partials_skip = 3;
		float num_used_partials = 0;
		for (int i = 1; i < NUM_OSC; i++) {
			/*
			float o = getOvertone(i, phase01, -offset * TWOPI_F);

			offset += phaseInc;
			if (offset > 1.0f) {
				offset = 0;
				phaseInc += phaseIncParam;
			}
			fOut += o;
			*/
			//float freq = powf(2.0f, fCV + fm) * (32.703251f * 0.5f);
			float freq = powf(2.0f, fCV + fm) * (32.703251f * 0.5f);
			osc[i].SetFreq(freq);
			partials[i] = osc[i].Process();
			if (((partials_start + i) % partials_skip) == 0) {
				fOut += partials[i];
				num_used_partials += 1.0f;
			}
			//fCV += inc;
			fCV += 0.1f + phaseIncParam;
		}
		//DEBUG("%f", fOut);

		phase01 += delta;
		phase01 = kbEucMod(phase01, 1.0f);


		//fOut /= (float)NUM_OSC;
		fOut /= num_used_partials; 
		float bufferOut = buffer[buffer_idx];
		outputs[FM_LEFT_OUTPUT].setVoltage(bufferOut);
		outputs[FM_RIGHT_OUTPUT].setVoltage(fOut * 2.5f);
		buffer_idx++;
		if (buffer_idx > 4096)
			buffer_idx = 0;
	}
};

struct PartialsWidget : ModuleWidget {
	Label* labelPitch;

	PartialsWidget(Partials* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/tzfmlead5HP.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));

		float spacingX = box.size.x / (float)4.0f;
		int x0 = spacingX;
		int x1 = box.size.x - spacingX;

		float y = 72;
		addParam(createParamCentered<RoundBlackKnob>(Vec(x0, y), module, Partials::PHASE_OFFSET_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(Vec(x1, y), module, Partials::PHASE_INC_PARAM));
		//addInput(createInputCentered<PJ301MPort>(Vec(x1, y), module, Partials::PHASER_WIDTH_INPUT));

		y = 142;
		//addParam(createParamCentered<RoundBlackKnob>(Vec(x0, y), module, Partials::PHASE_OFFSET_PARAM));
		//addInput(createInputCentered<PJ301MPort>(Vec(x1, y), module, Partials::PHASER_OFFSET_INPUT));

		y = 190;
		//addParam(createParamCentered<RoundBlackKnob>(Vec(x0, y), module, Partials::FM_FREQ_PARAM));
		//addParam(createParamCentered<RoundBlackKnob>(Vec(x1, y), module, Partials::FM_AMOUNT_PARAM));

		y = 238;
		//addParam(createParamCentered<RoundBlackKnob>(Vec(x0, y), module, Partials::MODULATOR_OCTAVE_PARAM));
		//addParam(createParamCentered<RoundBlackKnob>(Vec(x1, y), module, Partials::CARRIER_OCTAVE_PARAM));

		y = 296;
		//addOutput(createOutputCentered<PJ301MPort>(Vec(x0, y), module, Partials::MODULATOR_OUTPUT));
		//addOutput(createOutputCentered<PJ301MPort>(Vec(x1, y), module, Partials::CARRIER_OUTPUT));
		addParam(createParamCentered<RoundBlackKnob>(Vec(x0, y), module, Partials::FM_FREQ_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(Vec(x1, y), module, Partials::FM_AMOUNT_PARAM));

		y = 344;
		addOutput(createOutputCentered<PJ301MPort>(Vec(x0, y), module, Partials::FM_LEFT_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(x1, y), module, Partials::FM_RIGHT_OUTPUT));

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
		Partials * module = dynamic_cast<Partials*>(this->module);
		if (module) {
			labelPitch->text = module->getRootNoteName();
		}
		ModuleWidget::step();
	}
};

Model* modelPartials = createModel<Partials, PartialsWidget>("partials");
