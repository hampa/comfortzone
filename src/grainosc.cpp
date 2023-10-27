#include "plugin.hpp"
#include "oscillator.h"
#include <math.h>

#define LERP(a, b, f) (a * (1.0f - f)) + (b * f)
#define CLAMP(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

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
		MORPH_INPUT,
		WARP_INPUT,
		GATE_INPUT,
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
		WAVETABLE_SUNDIAL2,
		WAVETABLE_SUNDIAL3,
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

	float getSinePartial3(float phase01, float bin0Vol, int binA, float binAVol, int binB, float binBVol) {
		float phase = 2.0f * M_PI * phase01; 
		float f0 = sinf(phase) * bin0Vol;
		float f1 = sinf(phase * binA) * binAVol;
		float f2 = sinf(phase * binB) * binBVol;
		//return LERP(f1, f2, 0.5f);
		return f0 + f1 + f2;
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
		configInput(MORPH_INPUT, "Morph");
		configInput(WARP_INPUT, "Warp");
		configInput(GATE_INPUT, "Gate");
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

	inline float getPureSine(float phase01, float mult = 1) {
		return sinf(2.0f * M_PI * phase01 * mult); 
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

	/*
	float quantizeMultiplierToMinorScale(float mult) {
		// Map mult to 0 to 1 range (assuming mult is provided in this range).
		int index = (int)(mult * 6.9999); // 6.9999 is a slight bias to ensure proper rounding

		return noteMultipliers[index];
	}
	*/


	float noteMultipliers4up[28] = {
		// 1st octave
		1.0, 1.1225, 1.1892, 1.3348, 1.4983, 1.5874, 1.7818,
		// 2nd octave
		2.0, 2.245, 2.3784, 2.6696, 2.9966, 3.1748, 3.5636,
		// 3rd octave
		4.0, 4.49, 4.7568, 5.3392, 5.9932, 6.3496, 7.1272,
		// 4th octave
		8.0, 8.98, 9.5136, 10.6784, 11.9864, 12.6992, 14.2544
	};

	int mapKnobToIndex(float value, float centerIdx = 14, float length = 43) {
		if (value <= 0.5) {
			return (int)(value * centerIdx * 2.0f);  // 28 is 2 times 14, which gives the scaling factor for the first half.
		} 
		return centerIdx + (int)((value - 0.5) * (length - 1 - centerIdx));  // 54 is 2 times (41-14), which gives the scaling factor for the second half.
	}


	float minorMultiplier_OLD[43] = {
		// 2nd octave down
		0.25, 0.280625, 0.2973, 0.3337, 0.374575, 0.39685, 0.44545,
		// 1st octave down
		0.5, 0.56125, 0.5946, 0.6674, 0.74915, 0.7937, 0.8909,
		// Base octave
		1.0, 1.1225, 1.1892, 1.3348, 1.4983, 1.5874, 1.7818,
		// 1st octave up
		2.0, 2.245, 2.3784, 2.6696, 2.9966, 3.1748, 3.5636,
		// 2nd octave up
		4.0, 4.49, 4.7568, 5.3392, 5.9932, 6.3496, 7.1272,
		// 3rd octave up
		8.0, 8.98, 9.5136, 10.6784, 11.9864, 12.6992, 14.2544,
		// and one note
		9.0f
	};


	float minorMultiplier[85] = {
		0.5000,     0.5612,     0.5946,     0.6674,     0.7491,     0.7937,     0.8909,
		1.0000,     1.1225,     1.1892,     1.3348,     1.4983,     1.5874,     1.7818,
		2.0000,     2.2450,     2.3784,     2.6696,     2.9966,     3.1748,     3.5636,
		4.0000,     4.4900,     4.7568,     5.3392,     5.9932,     6.3496,     7.1272,
		8.0000,     8.9800,     9.5136,     10.6784,     11.9864,     12.6992,     14.2544,
		16.0000,     17.9600,     19.0272,     21.3568,     23.9728,     25.3984,     28.5088,
		32.0000,     35.9200,     38.0544,     42.7136,     47.9456,     50.7968,     57.0176,
		64.0000,     71.8400,     76.1088,     85.4272,     95.8912,     101.5936,     114.0352,
		128.0000,     143.6800,     152.2176,     170.8544,     191.7824,     203.1872,     228.0704,
    		256.0000,  287.3600,     304.4352,     341.7088,     383.5648,     406.3744,     456.1408,
		512,     574.7200,     608.8704,     683.4176,     767.1296,     812.7488,     912.2816,
    		1024.0000,     1149.4399,     1217.7408,     1366.8352,     1534.2592,     1625.4976,     1824.5632,
		2048
	};

	/*
	float arpeggioNoteMultipliers[36] = {
		// 1 octave down
		0.5, 0.5946, 0.74915, 0.8909,
		// Base octave
		1.0, 1.1892, 1.4983, 1.7818,
		// 1 octave up
		2.0, 2.3784, 2.9966, 3.5636,
		// 2 octaves up
		4.0, 4.7568, 5.9932, 7.1272,
		// 3 octaves up
		8.0, 9.5136, 11.9864, 14.2544,
		// 4 octaves up
		16.0, 19.0272, 23.9728, 28.5088,
		// 5 octaves up
		32.0, 38.0544, 47.9456, 57.0176,
		// 6 octaves up
		64.0, 76.1088, 95.8912, 114.0352,
		// 7 octaves up
		128.0, 152.2176, 191.7824, 228.0704,
		// 8 octaves up
    		256.0, 304.4352, 383.5648, 456.1408
	};
	*/

	float mapArpKnob(float value) {
		int idx = mapKnobToIndex(value, 8, 25);
		return arpMultiplier[idx];
	}

	float mapMinorKnob(float value) {
		//int idx = mapKnobToIndex(value, 14, 43);
		int idx = mapKnobToIndex(value, 7, 85);
		return minorMultiplier[idx];
	}

	float setMorphSlew(bool reset, float target_value, float slew_up_rate, float slew_down_rate) {
		static float current_value = 0.0f;
		if (reset) {
			current_value = target_value;
		}
		float slew_rate = (target_value > current_value) ? slew_up_rate : slew_down_rate;
		current_value += slew_rate * (target_value - current_value);
		return current_value;
	}

	float setSlew(bool reset, float target_value, float slew_up_rate, float slew_down_rate) {
		static float current_value = 0.0f;
		if (reset) {
			current_value = target_value;
		}

		// Calculate the difference
		float difference = target_value - current_value;

		// Use the absolute difference to modify the slew rate
		float adaptive_slew = 1.0f + fabs(difference) * 0.2f; // the factor "0.1f" can be adjusted as per the desired responsiveness

		// Decide on direction (up or down)
		float base_slew_rate = (difference > 0) ? slew_up_rate : slew_down_rate;

		// Apply the adaptive rate
		float slew_rate = base_slew_rate * adaptive_slew;

		current_value += slew_rate * difference;

		return current_value;
	}


	// CENTER 8
	float arpMultiplier[25] = {
		// 2 octaves down
		0.25, 0.2973, 0.374575, 0.44545,
		// 1 octave down
		0.5, 0.5946, 0.74915, 0.8909,
		// Base octave
		1.0, 1.1892, 1.4983, 1.7818,
		// 1 octave up
		2.0, 2.3784, 2.9966, 3.5636,
		// 2 octaves up
		4.0, 4.7568, 5.9932, 7.1272,
		// 3 octaves up
		8.0, 9.5136, 11.9864, 14.2544,
		9.0f
	};

	struct sundial_t {
		int binA;
		int binB;
	};
	struct sundial_t sd2[NUM_FIBOGLIDE] = {
		{ 5, 12 },
		{ 5, 10 },
		{ 3, 8 },
		{ 5, 9 },
		{ 4, 10 },
		{ 4, 14 },
		{ 7, 18 },
		{ 6, 20 },
		{ 7, 16 },
		{ 9, 24 },
		{ 3, 12 },
		{ 5, 8 }
	};

	struct sundial_t sd3[NUM_FIBOGLIDE] = {
		{ 5, 8 },
		{ 7, 10 },
		{ 7, 16 },
		{ 11, 19 },
		{ 14, 18 },
		{ 13, 18 },
		{ 17, 22 },
		{ 16, 24 },
		{ 7, 18 },
		{ 10, 18 },
		{ 18, 21 },
		{ 12, 22 }
	};


	inline float getSundial2(float phase01, float wt) {
		int idxA = floorf(wt);
		int idxB = idxA + 1;
		float rem = wt - floorf(wt);
		if (idxA < 0) {
			idxA = NUM_FIBOGLIDE - 1;
			idxB = 0;
		}
		if (idxB > NUM_FIBOGLIDE) {
			idxB = 0;
		}
		float a = getSinePartial3(phase01, 0.15f, sd2[idxA].binA, 0.5f, sd2[idxA].binB, 0.5f);
		float b = getSinePartial3(phase01, 0.15f, sd2[idxB].binA, 0.5f, sd2[idxB].binB, 0.5f);
		float result = LERP(a, b, rem);
		return result;
	}

	inline float getSundial3(float phase01, float wt) {
		int idxA = floorf(wt);
		int idxB = idxA + 1;
		float rem = wt - floorf(wt);
		if (idxA < 0) {
			idxA = NUM_FIBOGLIDE - 1;
			idxB = 0;
		}
		if (idxB > NUM_FIBOGLIDE) {
			idxB = 0;
		}
		float a = getSinePartial3(phase01, 0.5f, sd2[idxA].binA, 0.37f, sd2[idxA].binB, 0.37f);
		float b = getSinePartial3(phase01, 0.5f, sd2[idxB].binA, 0.37f, sd2[idxB].binB, 0.37f);
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
	float prevPhase02;
	float morph = 0;
	//float warpLatched = 0;
	//float morphLatched = 0;
	float outAmplitude = 5.0f;
	bool gate = false;
	bool prevGate = false;
	int grain_odd_idx = 0;
	int grain_even_idx = 0;
	int grain_idx = 0;
	float warp;
	float morph_target = 0;

	float keytrackingMultiplier(float rootNote, float currentMidiNote) {
		float interval = currentMidiNote - rootNote;
		return powf(2.0f, -interval / 12.0f);
	}

	void process(const ProcessArgs& args) override {
		float root_midi = 21.0f;
		float coarse = floorf(params[FREQ_PARAM].getValue() * 12); 
		float freqMidi = root_midi + coarse;
		fmNote = floorf(root_midi + coarse);

		//float freqMidi = floorf(params[FREQ_PARAM].getValue() * 127.0f - 64.0f);
		//float baseMidi = params[BASEFREQ_PARAM].getValue() * 127.0f - 64.0f;
		float amount = params[AMOUNT_PARAM].getValue();
		int osc = floorf(params[OSC_PARAM].getValue());
		int wavetable = floorf(params[WAVETABLE_PARAM].getValue());

		bool got_sh = false; 
		if (inputs[GATE_INPUT].isConnected()) {
			float gateInput = fabs(inputs[GATE_INPUT].getVoltage(0));
			gate = (gateInput > 1);
			if (gate) {
				if (prevGate != gate) {
					oscAM.Reset();
					oscRoot.Reset();
					got_sh = true;
				}
			}
			prevGate = gate;
		}
		else {
			gate = true;
			prevGate = true;
		}

		warp = params[WARP_PARAM].getValue();
		if (inputs[WARP_INPUT].isConnected()) {
			float warpInput = inputs[WARP_INPUT].getVoltage(0) / 5.0f;
			warp = CLAMP((warp + warpInput), 0, 1);
		}

		morph_target = params[MORPH_PARAM].getValue();
		if (inputs[MORPH_INPUT].isConnected()) {
			float morphInput = inputs[MORPH_INPUT].getVoltage(0) / 5.0f;
			morph_target = CLAMP((morph_target + morphInput), 0, 1);
		}
		morph = setMorphSlew(got_sh, morph_target, 0.002f, 0.002f);


		// 0.1 = 0 
		// 0.24 = 32 

		//int wave = floorf(params[MORPH_PARAM].getValue());
		float freq = mtof(freqMidi);
		//float baseFreq = mtof(baseMidi);

		//freq = 65.40639f;
		//oscRoot.SetFreq(freq);
		//oscRoot.SetFreq(rootFreq);
		float keytrack = 1.0f;
		float multiplier = 0;
		if (osc == OSC_GRAIN) {
			oscRoot.SetFreq(freq);
			//int idx = mapKnobToIndex(warp);
			//multiplier = noteMultipliers[idx];
			//multiplier = mapArpKnob(warp);
			float m = mapMinorKnob(warp);
			//int grain_even_idx = mapKnobToIndex(warp, 7, 85);
			//grain_odd_idx = grain_idx - 1;
			//if (grain_odd_idx < 0) {
		//		grain_odd_idx = 0;
		//	}

			//float target = minorMultiplier[grain_even_idx];
			multiplier = setSlew(got_sh, m, 0.0001, 0.0001);
			//0.94387
			//multiplier = noteMultipliers[idx];
			//multiplier = LERP(0.5, 8, warp);
			//multiplier = quantizeMultiplierToMinorScale(multiplier);
		}
		else {
			oscRoot.SetFreq(65);
			keytrack = keytrackingMultiplier(21+6, freqMidi);
			//multiplier = LERP(1, 8, warp);
			//multiplier = quantizeMultiplierToMinorScale(multiplier);
			float m = mapMinorKnob(warp);
			multiplier = setSlew(got_sh, m, 0.0001, 0.0001);
			//int idx = mapKnobToIndex(warp);
			//multiplier = noteMultipliers[idx];
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
		if (prevPhase02 > phase02) {
			//morph = morphLatched;
			//warp = warpLatched;
		}
		if (gate) {
			outAmplitude = 5.0f;
		}
		else {
			outAmplitude *= 0.99f;
		}
		prevPhase02 = phase02;
		
		float phase = oscAM.GetPhase() * 2.0f;
		//float offset = M_PI * amount; //M_PI/2;
		float offset = M_PI/2.0f;
					    //
				     //i 
		float sinus1 = sinf(phase - offset);
		float sinus3 = sinf((phase - offset) * 3.0f);
		float sinus = LERP(sinus1, sinus3, 0.1f);
		float outAM2 = (sinus + 0.80f) * 0.625f;
		if (outAM2 < 0.0001f) {
			outAM2 = 0;
		}
		//float sa = 1.0f / 1.1f;
		//float sb = 0.1f / 1.1f;
		//float sinus = sa * sinus1 + sb * sinus3;
		//float outAM2 = (sinus + 1) * 0.5f;

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
				outR = getFibo(phase01, morph * (NUM_FIBOGLIDE - 1)) * am;
				break;
			case WAVETABLE_HARMONICSWEEP:
				outR = getHarmonicSweep(phase01, morph * (NUM_FIBOGLIDE - 1)) * am;
				break;
			case WAVETABLE_SUNDIAL2:
				outR = getSundial2(phase01, morph * (NUM_FIBOGLIDE - 1)) * am;
				break;
			case WAVETABLE_SUNDIAL3:
				outR = getSundial3(phase01, morph * (NUM_FIBOGLIDE - 1)) * am;
				break;
			default:
				outR = getPureSine(phase01) * am;
				break;
		}
		float debug = phase02; //getPureSine(phase01);

		outputs[LEFT_OUTPUT].setVoltage(outAM2 * 5.0f);
		outputs[RIGHT_OUTPUT].setVoltage(outR * outAmplitude);
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
		addInput(createInputCentered<PJ301MPort>(Vec(x1, y), module, GrainOsc::GATE_INPUT));
		//addParam(createParamCentered<RoundBlackKnob>(Vec(x1, y), module, GrainOsc::ODD_DETUNE_PARAM));

		y = 190;
		addParam(createParamCentered<RoundBlackKnob>(Vec(x0, y), module, GrainOsc::WARP_PARAM)); // PW
		addInput(createInputCentered<PJ301MPort>(Vec(x1, y), module, GrainOsc::WARP_INPUT));
		//addParam(createParamCentered<RoundBlackKnob>(Vec(x0, y), module, GrainOsc::AMOUNT_PARAM)); // PW

		y = 238;
		addParam(createParamCentered<RoundBlackKnob>(Vec(x0, y), module, GrainOsc::MORPH_PARAM));
		addInput(createInputCentered<PJ301MPort>(Vec(x1, y), module, GrainOsc::MORPH_INPUT));

		y = 296;
		//addOutput(createOutputCentered<PJ301MPort>(Vec(x0, y), module, GrainOsc::MODULATOR_OUTPUT));
		//addOutput(createOutputCentered<PJ301MPort>(Vec(x1, y), module, GrainOsc::AM_OUTPUT));
		addParam(createParamCentered<RoundBlackKnob>(Vec(x0, y), module, GrainOsc::AMOUNT_PARAM)); // PW
		addOutput(createOutputCentered<PJ301MPort>(Vec(x1, y), module, GrainOsc::AM2_OUTPUT));

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
