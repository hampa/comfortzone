#include "plugin.hpp"
#include "oscillator.h"
#include <math.h>

#define LERP(a, b, f) (a * (1.0f - f)) + (b * f)

struct GrainOsc : Module {
	enum ParamIds {
		FREQ_PARAM,
		AMOUNT_PARAM,
		WARP_PARAM,
		MORPH_PARAM,
		OSC_PARAM,
		WAVETABLE_PARAM,
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

	enum {
		OSC_GRAIN,
		OSC_FORMANT,
		NUM_OSC
	};
	enum {
		WAVETABLE_SIN,
		WAVETABLE_FIBOGLIDE,
		WAVETABLE_HARMONICSWEEP,
		NUM_WAVETABLES
	};

	inline float fmap(float in, float min, float max) {
		return min + in * (max - min), min, max;
	}

	//#define BUFFER_SIZE 2048
	#define BUFFER_SIZE 4096 
	void fillSineWave(float* buffer, int length, float mult, float inv = 1) {
    		for (int i = 0; i < length; i++) {
        		// Compute the phase for the current sample.
        		// This will go from 0 to 2*PI over the length of the buffer.
        		float phase = 2.0f * M_PI * (float)i / (float)length;

        		// Fill the buffer with sine wave values
        		//buffer[i] = sinf(phase * mult) * inv;
        		buffer[i] = getSinePartial((float) i / (float)length, mult, inv); 
    		}
	}
	
	float getSinePartial(float phase01, float bin, float inv = 1) {
		float phase = 2.0f * M_PI * phase01; 
		return sinf(phase * bin) * inv;
	}

	float getSinePartial2(float phase01, int bin) {
		float phase = 2.0f * M_PI * phase01; 
		float inv = 1.0f;
		if ((bin % 2) == 1) {
			inv = -1;
		}
		float f1 = sinf(phase * bin) * inv;
		float f2 = sinf(phase * bin + 2) * inv;
		//return LERP(f1, f2, 0.5f);
		return f1 + f2;
	}

	float sinebuf[BUFFER_SIZE];
	float sinebuf3[BUFFER_SIZE];
#define NUM_FIBOGLIDE 12
	int currentFibo = 0;
	float fibobuf[NUM_FIBOGLIDE][BUFFER_SIZE];
	int fibobin[NUM_FIBOGLIDE] = { 1,2,2,3,5,8,13,21,34,55,89,144 };
	float fiboinv[NUM_FIBOGLIDE] = { 1,-1,-1,1, 1,-1,1,1,-1,1,1,-1 };

	GrainOsc () {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(FREQ_PARAM, 0.f, 1.f, 0.5f, "Freq", "%", 0.f, 100.f);
		//configParam(BASEFREQ_PARAM, 0.f, 1.f, 0.5f, "Base Freq", "%", 0.f, 100.f);
		configParam(AMOUNT_PARAM, 0.f, 1.f, 0.5f, "Amount");
		configParam(WARP_PARAM, 0.f, 1.0f, 0.5f, "Warp");
		//configParam(MORPH_PARAM, 0.f, NUM_FIBOGLIDE - 1, 0.0f, "Morph");
		configParam(OSC_PARAM, 0.f, NUM_OSC - 1, 0.0f, "Osc");
		configParam(MORPH_PARAM, 0.f, 1.0f, 0.0f, "Morph");
		configParam(WAVETABLE_PARAM, 0.f, NUM_WAVETABLES - 1, 0.0f, "wavetable");
		configInput(PHASER_WIDTH_INPUT, "Phaser Width FM");
		configInput(PHASER_OFFSET_INPUT, "Phaser Offset FM");
		fillSineWave(sinebuf, BUFFER_SIZE, 1);
		fillSineWave(sinebuf3, BUFFER_SIZE, 3);

		fillSineWave(fibobuf[0], BUFFER_SIZE, 1);
		fillSineWave(fibobuf[1], BUFFER_SIZE, 2, -1);
		fillSineWave(fibobuf[2], BUFFER_SIZE, 2, -1);
		fillSineWave(fibobuf[3], BUFFER_SIZE, 3);
		fillSineWave(fibobuf[4], BUFFER_SIZE, 5);
		fillSineWave(fibobuf[5], BUFFER_SIZE, 8);
		fillSineWave(fibobuf[6], BUFFER_SIZE, 13);
		fillSineWave(fibobuf[7], BUFFER_SIZE, 21);
		fillSineWave(fibobuf[8], BUFFER_SIZE, 34);
		fillSineWave(fibobuf[9], BUFFER_SIZE, 55);
		fillSineWave(fibobuf[10], BUFFER_SIZE, 89);
		fillSineWave(fibobuf[11], BUFFER_SIZE, 144);
	}

	~GrainOsc () {
	}

	const char *getNoteName(int i) {
		const char *notes[] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "G#", "A", "Bb", "B" };
		return notes[(i + 12) % 12];
	}
	Oscillator oscRoot;
	//Oscillator oscFormant;
	Oscillator oscAM;

	int fmNote = 0;
	const char *getRootNoteName() {
		return getNoteName(fmNote);
	}


	void onSampleRateChange(const SampleRateChangeEvent& e) override {
		//samplerateInv = 1.0f / (float)e.sampleRate;
		oscRoot.Init(e.sampleRate);
		oscRoot.SetWaveform(Oscillator::WAVE_SIN);
		//oscFormant.Init(e.sampleRate);
		//oscFormant.SetWaveform(Oscillator::WAVE_SIN);
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

	inline float getFibo(float phase01, float wt) {
		int idxA = floorf(wt);
		//idxA %= NUM_FIBOGLIDE;
		int idxB = idxA + 1;
		float rem = wt - floorf(wt);
		if (idxA < 0) {
			idxA = NUM_FIBOGLIDE - 1;
			idxB = 0;
		}
		if (idxB > NUM_FIBOGLIDE) {
			idxB = 0;
		}
		//float a = cubicInterpolate(fibobuf[idxA], phase01);
		//float b = cubicInterpolate(fibobuf[idxB], phase01);
		int binA = fibobin[idxA];
		int binB = fibobin[idxB];
		float invA = fiboinv[idxA];
		float invB = fiboinv[idxB];
		float a = getSinePartial(phase01, binA, invA);
		float b = getSinePartial(phase01, binB, invB);
		float result = LERP(a, b, rem);
		return result;
	}

	inline float getPureSine(float phase01) {
		return sinf(2.0f * M_PI * phase01); 
	}

	inline float getHarmonicSweep(float phase01, float wt) {
		int idxA = floorf(wt);
		//idxA %= NUM_FIBOGLIDE;
		int idxB = idxA + 1;
		float rem = wt - floorf(wt);
		if (idxA < 0) {
			idxA = NUM_FIBOGLIDE - 1;
			idxB = 0;
		}
		if (idxB > NUM_FIBOGLIDE) {
			idxB = 0;
		}
		float a = getSinePartial2(phase01, idxA + 1);
		float b = getSinePartial2(phase01, idxB + 1);
		float result = LERP(a, b, rem);
		return result;
	}


	float cubicInterpolate(const float* buffer, float position) {
		// Get the integer and fractional parts of the position.
		int32_t intPos = (int32_t)position;
		float fracPos = position - intPos;
		int x0 = intPos - 1;
		int x1 = intPos;
		int x2 = intPos + 1;
		int x3 = intPos + 2;
		if (x0 < 0) {
			x0 = BUFFER_SIZE - 1;
		}
		else if (x2 > BUFFER_SIZE - 1) {
			x2 = 0;
			x3 = 1;
		}
		else if (x3 > BUFFER_SIZE - 1) {
			x3 = 0;
		}

		// Retrieve the 4 samples that will be used for cubic interpolation.
		//float y0 = buffer[intPos - 1];
		//float y1 = buffer[intPos];
		//float y2 = buffer[intPos + 1];
		//float y3 = buffer[intPos + 2];
		float y0 = buffer[x0];
		float y1 = buffer[x1];
		float y2 = buffer[x2];
		float y3 = buffer[x3];

		// Cubic interpolation coefficients.
		float a0 = y3 - y2 - y0 + y1;
		float a1 = y0 - y1 - a0;
		float a2 = y2 - y0;
		float a3 = y1;

		// Return interpolated value.
		return (a0 * fracPos * fracPos * fracPos) + (a1 * fracPos * fracPos) + (a2 * fracPos) + a3;
	}


	float prevAmPhase;
	float prevRootPhase;

	float keytrackingMultiplier(float rootNote, float currentMidiNote) {
		float interval = currentMidiNote - rootNote;
		return powf(2.0f, -interval / 12.0f);
	}

	void process(const ProcessArgs& args) override {
		float root_midi = 21.0f;
		float coarse = floorf(params[FREQ_PARAM].getValue() * 12); 
		float freqMidi = root_midi + coarse;
		//float freqMidi = floorf(params[FREQ_PARAM].getValue() * 127.0f - 64.0f);
		//float baseMidi = params[BASEFREQ_PARAM].getValue() * 127.0f - 64.0f;
		float amount = params[AMOUNT_PARAM].getValue();
		float warp = params[WARP_PARAM].getValue();
		int osc = floorf(params[OSC_PARAM].getValue());
		int wavetable = floorf(params[WAVETABLE_PARAM].getValue());

		// 0.1 = 0 
		// 0.24 = 32 

		//int wave = floorf(params[MORPH_PARAM].getValue());
		float wave = params[MORPH_PARAM].getValue();
		float freq = mtof(freqMidi);
		//float baseFreq = mtof(baseMidi);

		//freq = 65.40639f;
		//oscRoot.SetFreq(freq);
		//oscRoot.SetFreq(rootFreq);
		float keytrack = 1.0f;
		float multiplier = 0;
		if (osc == OSC_GRAIN) {
			oscRoot.SetFreq(freq);
			multiplier = LERP(0.5, 8, warp);
		}
		else {
			oscRoot.SetFreq(65);
			keytrack = keytrackingMultiplier(21+6, freqMidi);
			multiplier = LERP(1, 8, warp);
		}

		oscAM.SetFreq(freq * 0.5f);

		float outL = oscRoot.Process();
		float outAM = fabs(oscAM.Process());
		float amPhase = oscAM.GetPhase();
		float rootPhase = oscRoot.GetPhase();
		/*
		if (osc == OSC_GRAIN) {
			if (rootPhase < prevRootPhase) {
				oscAM.Reset();
			}
		}
		else {
			if (amPhase < prevAmPhase) {
				oscRoot.Reset();
			}
		}
		*/
		/*
		if (amPhase < prevAmPhase) {
			oscRoot.Reset();
		}
		*/
			
		prevAmPhase = amPhase;
		prevRootPhase = rootPhase;

		//float phase01 = oscRoot.GetPhase01();
		//float phase02 = oscAM.GetPhase01() * 2.0f;
		float phase02 = oscAM.GetPhase01();
		float phase = oscAM.GetPhase() * 2.0f;
		//float offset = M_PI * amount; //M_PI/2;
		float offset = M_PI/2.0f;
					    //
				     //i 
		float sinus1 = sinf(phase - offset);
		float sinus3 = sinf((phase - offset) * 3.0f);
		float sinus = LERP(sinus1, sinus3, 0.1f);
		float outAM2 = (sinus + 0.81f) * 0.5f;

		//float phase01 = phase02 + 1; //phase02 + 0.5;
		//float outFibo = getFibo(phase01 * (BUFFER_SIZE - 1), wave * (NUM_FIBOGLIDE - 1));
		//
		float squeeze = phase02 * (1 + 7 * warp);
		//float freqPhase = fmod(phase02 * 2 + 0.5, 1);
		float freqPhase = fmod(phase02 * 2 + 0.5, 1);
		if (freqPhase > 0.5f) {
			freqPhase = (1 - freqPhase) * -1;
		}
		float phase01 = freqPhase * multiplier * keytrack;
		 
		float outR = 0;
		float am = outAM2;
		switch (wavetable) {
			case WAVETABLE_FIBOGLIDE:
				outR = getFibo(phase01, wave * (NUM_FIBOGLIDE - 1)) * am;
				break;
			case WAVETABLE_HARMONICSWEEP:
				outR = getHarmonicSweep(phase01, wave * (NUM_FIBOGLIDE - 1)) * am;
				break;
			default:
				outR = getPureSine(phase01) * am;
				break;
		}
		float debug = keytrack; //getPureSine(phase01);

		outputs[LEFT_OUTPUT].setVoltage(outAM2 * 5.0f);
		outputs[RIGHT_OUTPUT].setVoltage(outR * 5.0f);
		outputs[AM_OUTPUT].setVoltage(freqPhase * 5.0f);
		outputs[AM2_OUTPUT].setVoltage(debug * 5.0f);
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
		addParam(createParamCentered<RoundBlackKnob>(Vec(x1, y), module, GrainOsc::OSC_PARAM));
	//	addInput(createInputCentered<PJ301MPort>(Vec(x1, y), module, GrainOsc::PHASER_WIDTH_INPUT));

		y = 142;
		addParam(createParamCentered<RoundBlackKnob>(Vec(x0, y), module, GrainOsc::WAVETABLE_PARAM));
		//addParam(createParamCentered<RoundBlackKnob>(Vec(x1, y), module, GrainOsc::ODD_DETUNE_PARAM));

		y = 190;
		addParam(createParamCentered<RoundBlackKnob>(Vec(x0, y), module, GrainOsc::AMOUNT_PARAM)); // PW
		addParam(createParamCentered<RoundBlackKnob>(Vec(x1, y), module, GrainOsc::WARP_PARAM)); // PW

		y = 238;
		addParam(createParamCentered<RoundBlackKnob>(Vec(x0, y), module, GrainOsc::MORPH_PARAM));
		//addParam(createParamCentered<RoundBlackKnob>(Vec(x1, y), module, GrainOsc::AM_MORPH_PARAM));

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
