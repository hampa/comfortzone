#include "plugin.hpp"
#include "oscillator.h"
#include <math.h>

#define LERP(a, b, f) (a * (1.0f - f)) + (b * f)

struct GrainOsc : Module {
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
		AM_OUTPUT,
		AM2_OUTPUT,
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

	#define BUFFER_SIZE 2048
	void fillSineWave(float* buffer, int length, float mult) {
    		for (int i = 0; i < length; i++) {
        		// Compute the phase for the current sample.
        		// This will go from 0 to 2*PI over the length of the buffer.
        		float phase = 2.0f * M_PI * (float)i / (float)length;

        		// Fill the buffer with sine wave values
        		buffer[i] = sinf(phase * mult);
    		}
	}

	float sinebuf[BUFFER_SIZE];
	float sinebuf3[BUFFER_SIZE];

	GrainOsc () {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(FREQ_PARAM, 0.f, 1.f, 0.5f, "Freq", "%", 0.f, 100.f);
		configParam(AMOUNT_PARAM, 0.f, 1.f, 0.5f, "Amount");
		configParam(WARP_PARAM, 0.f, 1.0f, 0.5f, "Warp");
		configParam(WAVE_PARAM, 0.f, NUM_WAVES - 1, 0.0f, "Wave");
		configInput(PHASER_WIDTH_INPUT, "Phaser Width FM");
		configInput(PHASER_OFFSET_INPUT, "Phaser Offset FM");
		fillSineWave(sinebuf, BUFFER_SIZE, 1);
		fillSineWave(sinebuf3, BUFFER_SIZE, 3);
	}

	~GrainOsc () {
	}

	const char *getNoteName(int i) {
		const char *notes[] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "G#", "A", "Bb", "B" };
		return notes[(i + 12) % 12];
	}
	Oscillator oscRoot;
	Oscillator oscGrain;
	Oscillator oscAM;

	int fmNote = 0;
	const char *getRootNoteName() {
		return getNoteName(fmNote);
	}


	void onSampleRateChange(const SampleRateChangeEvent& e) override {
		//samplerateInv = 1.0f / (float)e.sampleRate;
		oscRoot.Init(e.sampleRate);
		oscRoot.SetWaveform(Oscillator::WAVE_SIN);
		oscGrain.Init(e.sampleRate);
		oscGrain.SetWaveform(Oscillator::WAVE_SIN);
		oscAM.Init(e.sampleRate);
		oscAM.SetWaveform(Oscillator::WAVE_SIN);
	}

	inline float mtof(float m) {
		return powf(2, (m - 69.0f) / 12.0f) * 440.0f;
	}


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


	const float kHalfPhase = 0.5f;
	const int kMaxSyncPower = 4;
	const int kMaxSync = 1 << kMaxSyncPower;

	float syncPhase(float phase, float warp) {
      		float f = (phase + kHalfPhase) * warp;
      		//return f * kMaxSync + kHalfPhase;
      		return f + kHalfPhase;
    	}

	float syncPhaseFUCKED(float phase, float warp) {
    		// Calculate new phase
    		float f = phase * warp;

    		// If you want to ensure the phase always starts at the midpoint, adjust the phase.
    		// This assumes kHalfPhase represents half of the waveform's total phase (e.g., PI for a sine wave).
    		if(f < kHalfPhase) {
        		return f + kHalfPhase;
    		}

		return f * kMaxSync;
	}


	float kbEucMod(float a, float b) {
		float mod = fmod(a, b);
		if (mod < 0.f) {
			mod += b;
		}
		return mod;
	}

	float prevAmPhase;
	void process(const ProcessArgs& args) override {
		float freqMidi = params[FREQ_PARAM].getValue() * 127.0f - 64.0f;
		float amount = params[AMOUNT_PARAM].getValue();
		float warp = params[WARP_PARAM].getValue();
		// 0.1 = 0 
		// 0.24 = 32 

		int wave = floorf(params[WAVE_PARAM].getValue());
		float freq = mtof(freqMidi);

		freq = 65.40639f;
		oscRoot.SetFreq(freq);
		oscAM.SetFreq(freq * 0.5f);

		float outL = oscRoot.Process();
		float outAM = fabs(oscAM.Process());
		float amPhase = oscRoot.GetPhase();
		if (amPhase < prevAmPhase) {
			oscAM.Reset();
		}
		prevAmPhase = amPhase;

		float phase01 = oscRoot.GetPhase01();
		//float distortedPhase = syncPhase(phase01, warp * 10);

		//float distPhaseScaled = distortedPhase; // * (1.0f + amount * 50.0f);
		float phase = oscRoot.GetPhase();
		float offset = M_PI/2;
		float sinus1 = sinf(phase - offset);
		//float sinus3 = sinf((phase * 3.0f) - offset);
		float sinus3 = sinf((phase - offset) * 3.0f);
		//float sinus = sinus1 + sinus3 * 0.1f; //amount; //LERP(sinus1, sinus3, amount);
		float sinus = LERP(sinus1, sinus3, 0.1f);
		float outAM2 = (sinus + 0.81f) * 0.5f;
	
		//int index = floorf(distortedPhase * BUFFER_SIZE);	
		/*
		phase01 -= 0.5f;
		if (phase01 < 0)
			phase01 += 0.5f;
		*/
		//phase01 = kbEucMod(phase01 - 0.5f, 1.0f);
		int index = floorf((phase01 - 0.5f) * BUFFER_SIZE * warp * 40);

		//int index = floorf(phase01 * BUFFER_SIZE);	
		if (index < 0) {
			index = BUFFER_SIZE - ((index * -1) % BUFFER_SIZE); 
			//index += BUFFER_SIZE;
		}
		float outR = sinebuf[index % BUFFER_SIZE] * outAM2;

		outputs[LEFT_OUTPUT].setVoltage(outL * 5.0f);
		outputs[RIGHT_OUTPUT].setVoltage(outR * 5.0f);
		outputs[AM_OUTPUT].setVoltage(outAM * 5.0f);
		outputs[AM2_OUTPUT].setVoltage(outAM2 * 5.0f);
	}
};

struct GrainOscWidget : ModuleWidget {
	Label* labelPitch;

	GrainOscWidget(GrainOsc* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/tzfmlead5HP.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));

		float spacingX = box.size.x / (float)4.0f;
		int x0 = spacingX;
		int x1 = box.size.x - spacingX;

		float y = 72;
		addParam(createParamCentered<RoundBlackKnob>(Vec(x0, y), module, GrainOsc::FREQ_PARAM));
		addInput(createInputCentered<PJ301MPort>(Vec(x1, y), module, GrainOsc::PHASER_WIDTH_INPUT));

		y = 142;
		//addParam(createParamCentered<RoundBlackKnob>(Vec(x0, y), module, GrainOsc::EVEN_DETUNE_PARAM));
		//addParam(createParamCentered<RoundBlackKnob>(Vec(x1, y), module, GrainOsc::ODD_DETUNE_PARAM));

		y = 190;
		addParam(createParamCentered<RoundBlackKnob>(Vec(x0, y), module, GrainOsc::AMOUNT_PARAM)); // PW
		addParam(createParamCentered<RoundBlackKnob>(Vec(x1, y), module, GrainOsc::WARP_PARAM)); // PW

		y = 238;
		addParam(createParamCentered<RoundBlackKnob>(Vec(x0, y), module, GrainOsc::WAVE_PARAM));
		//addParam(createParamCentered<RoundBlackKnob>(Vec(x1, y), module, GrainOsc::AM_WAVE_PARAM));

		y = 296;
		//addOutput(createOutputCentered<PJ301MPort>(Vec(x0, y), module, GrainOsc::MODULATOR_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(x0, y), module, GrainOsc::AM2_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(x1, y), module, GrainOsc::AM_OUTPUT));

		y = 344;
		addOutput(createOutputCentered<PJ301MPort>(Vec(x0, y), module, GrainOsc::LEFT_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(x1, y), module, GrainOsc::RIGHT_OUTPUT));

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
		GrainOsc * module = dynamic_cast<GrainOsc*>(this->module);
		if (module) {
			labelPitch->text = module->getRootNoteName();
		}
		ModuleWidget::step();
	}
};

Model* modelGrainOsc = createModel<GrainOsc, GrainOscWidget>("grainosc");
