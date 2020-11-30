#ifndef KICKBASS_H
#define KICKBASS_H

#include "biquad.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

//static const int UPSAMPLE = 2;
const int KB_OVERSAMPLE = 2;

struct point2 {
	float x;
	float y;
};

/*
float CLAMP(float x, float upper, float lower)
{
    return fmin(upper, fmax(x, lower));
}
*/

#define CLAMP(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

float kbEucMod(float a, float b) {
	float mod = std::fmod(a, b);
	//float mod = fmod(a, b);
	if (mod < 0.f) {
		mod += b;
	}
	return mod;
}

template <int OVERSAMPLE, int QUALITY>
struct KBVoltageControlledOscillator {
	float phase = 0.0f;
	float freq;
	float pw = 0.5f;
	float pitch;
	float voltage;
	float wavetable = 0;
	float morph = 0;

#ifdef WITH_DECIMATOR
	dsp::Decimator<KB_OVERSAMPLE, QUALITY> sinDecimator;
	dsp::Decimator<KB_OVERSAMPLE, QUALITY> sawDecimator;
	dsp::Decimator<KB_OVERSAMPLE, QUALITY> sqrDecimator;
	dsp::Decimator<KB_OVERSAMPLE, QUALITY> pulseDecimator;
#endif

	float sinBuffer[KB_OVERSAMPLE] = {};
	float sawBuffer[KB_OVERSAMPLE] = {};
	float sqrBuffer[KB_OVERSAMPLE] = {};
	float fmSawBuffer[KB_OVERSAMPLE] = {};
	float pulseBuffer[KB_OVERSAMPLE] = {};
	float additiveBuffer[KB_OVERSAMPLE] = {};

	float waveTable[2] = { 1.0, -1.0f };

	void setPitch(float octave, float pitchKnob, float pitchCv, float freqOffset) {
		// Compute frequency
		float pitch = 1.0 + roundf(octave);
		pitch += pitchKnob;
		pitch += pitchCv / 12.0;

		// Note C4
		freq = 261.626f * powf(2.0f, pitch) + freqOffset;
		freq = CLAMP(freq, 0.0f, 20000.0f);
	}

	float getPitchFreq(float octave, float pitchKnob, float pitchCv, float freqOffset) {
		float pitch = 1.0 + roundf(octave);
		int q = pitchKnob * 12;
		pitch += (float)q / 12.0f;
		pitch += pitchCv / 12.0;

		// Note C4
		float f = 261.626f * powf(2.0f, pitch) + freqOffset;
		f = CLAMP(f, 0.0f, 20000.0f);
		return f;	
	}

	// sigmode curve
	// https://www.desmos.com/calculator/aksjkh9das?lang=sv-SE
	void setPitchQ(float octave, float pitchKnob, float pitchCv, float freqOffset) {
		// Compute frequency
		float pitch = 1.0 + roundf(octave);
		int q = pitchKnob * 12;
		pitch += (float)q / 12.0f;
		pitch += pitchCv / 12.0;

		// Note C4
		freq = 261.626f * powf(2.0f, pitch) + freqOffset;
		freq = CLAMP(freq, 0.0f, 20000.0f);
	}

	void setPulseWidth(float pulseWidth) {
		const float pwMin = 0.01f;
		pw = CLAMP(pulseWidth, pwMin, 1.0f - pwMin);
	}


	float lerp(float a, float b, float f) {
		return (a * (1.0 - f)) + (b * f);
	}

	float getChirp(float _phase, float _morph) {
		float a = lerp(2, 12, _morph);
		float sin_val = std::sin(powf(_phase, 1.0f / M_PI) * M_PI * a);
		return sin_val;
	}

	float getClick(float _phase, float minFreq, float maxFreq, float _morph) {
		float a = lerp(minFreq, maxFreq, _morph);
		float sin_val = std::sin(powf(_phase, 1.0f / M_PI) * M_PI * a);
		return sin_val;
	}

	float getChirpOLD(float w1, float w2, float time) {
		float res;
		time = sqrt(time);
		res = cos(w1 * time + (w2 - w1) * time * time / 2);

		//res = A * Mathf.Cos(w1 * time + (w2 - w1) * time / (2 * M));
		return res;
	}

	float getSaw(float phase) {
		if (wavetable < 0.1f) {
			return lerp(-1.0f, 1.0f, phase);
		}
		else if (wavetable < 0.2f) {
			return lerp(-0.8f, 1.0f, phase);
		}
		else if (wavetable < 0.3f) {
			return lerp(1, -1.0f, phase);
		}
		return lerp(1, -0.8f, phase);
	}

	float getOvertone(int x, float _phase, float q) {
		float amp_weight = 1.0f / ((float)x - q);
		float overtone = amp_weight * sinf(2.0f * M_PI * _phase * x);
		//float overtone = amp_weight * getChirp(_phase * x, morph);
		return overtone;
	}
	//
	// phase goes from 0 to 1
	// output -1 to 1
	void processAdditive(float deltaTime, float numOvertones, float q) {
		if (numOvertones > 32)
			numOvertones = 32;
		// Advance phase
		float deltaPhase = CLAMP(freq * deltaTime, 1e-6, 0.5f);

		float numHarmonics = numOvertones + 1.0f;
		int count = floor(numHarmonics);
		float reminder = numHarmonics - count;

		for (int i = 0; i < KB_OVERSAMPLE; i++) {
			additiveBuffer[i] = 0;
			int lastHarmonic = 0;
			int lastFullHarmonic = 0;
			for (int x = 1; x < count; x++) {
				if (x == count -1) {
					additiveBuffer[i] += getOvertone(x, phase, 0); 
					lastFullHarmonic = x;
				}
				else {
					additiveBuffer[i] += getOvertone(x, phase, 0); 
				}
				lastHarmonic = x;
			}
			//DEBUG("%i %i", lastHarmonic, lastFullHarmonic);
			float o1 = getOvertone(lastHarmonic + 1, phase, 0);
			additiveBuffer[i] += o1 * reminder;
		
			// Advance phase
			phase += deltaPhase / KB_OVERSAMPLE;
			phase = kbEucMod(phase, 1.0f);

		}
	}

	void processSaw(float deltaTime) {
		float deltaPhase = CLAMP(freq * deltaTime, 1e-6, 0.5f);
		for (int i = 0; i < KB_OVERSAMPLE; i++) {
			sawBuffer[i] = getSaw(phase);
			phase += deltaPhase / KB_OVERSAMPLE;
			phase = kbEucMod(phase, 1.0f);
		}
	}

	float previousSample = 1;
	float bufferSample1 = 0.f;
	float bufferSample2 = 0.f;
	float feedbackSample = 0.f;

	void processFmSaw(float deltaTime, float feedbackAmount, float fmMod) {
		float deltaPhase = CLAMP(freq * deltaTime, 1e-6, 0.5f);

		//phase += freq * time + fmMod;

		for (int i = 0; i < KB_OVERSAMPLE; i++) {
			float sine = std::sin(2.f * M_PI * phase + feedbackAmount * feedbackSample);
			fmSawBuffer[i] = sine; 
			bufferSample1 = sine;
			bufferSample2 = bufferSample1; 
			feedbackSample = (bufferSample1 + bufferSample2) / 2.0f;

			phase += deltaPhase / KB_OVERSAMPLE;
			phase += fmMod / KB_OVERSAMPLE;
			phase = kbEucMod(phase, 1.0f);
		}
	}

	void processLerp(float deltaTime, float amount) {
		float deltaPhase = CLAMP(freq * deltaTime, 1e-6, 0.5f);
		for (int i = 0; i < KB_OVERSAMPLE; i++) {
			float sine = std::cos(2.0f * M_PI * phase);
			float saw = lerp(1.0f, -1.0f, phase); 
			fmSawBuffer[i] = lerp(sine, saw, amount * amount);
			phase += deltaPhase / KB_OVERSAMPLE;
			phase = kbEucMod(phase, 1.0f);
		}
	}

	void processPulse(float deltaTime) {
		float deltaPhase = CLAMP(freq * deltaTime, 1e-6, 0.5f);
		for (int i = 0; i < KB_OVERSAMPLE; i++) {
			pulseBuffer[i] = getChirp(phase, morph);
			phase += deltaPhase / KB_OVERSAMPLE;
			phase = kbEucMod(phase, 1.0f);
		}
	}


	// phase goes from 0 to 1
	// output -1 to 1
	void process(float deltaTime) {
		// Advance phase
		float deltaPhase = CLAMP(freq * deltaTime, 1e-6, 0.5f);

		//double instantaneous_w, current_phase;	

		for (int i = 0; i < KB_OVERSAMPLE; i++) {
			sinBuffer[i] = sinf(2.f*M_PI * phase);

			if (phase < 0.5f)
				sawBuffer[i] = 2.f * phase;
			else
				sawBuffer[i] = -2.f + 2.f * phase;

			sawBuffer[i] = getSaw(phase);

			sqrBuffer[i] = (phase < pw) ? 1.f : -1.f;

			pulseBuffer[i] = getChirp(phase, morph);

			//pulseBuffer[i] = sinf(current_phase); 

			// Advance phase
			phase += deltaPhase / KB_OVERSAMPLE;
			phase = kbEucMod(phase, 1.0f);

			// pulse phase
			//instantaneous_w = sweep_func (w0, w1, (1.0 * i) / OVERSAMPLE) ;
			//current_phase = fmod (current_phase + instantaneous_w, 2.0 * M_PI) ;

		}
		//DEBUG("phase %f sawbuffer %f oversample %i\n", phase, sawBuffer[0], OVERSAMPLE);
	}

	void resetPhase() {
#ifdef WITH_DECIMATOR
		sinDecimator.reset();
		sawDecimator.reset();
		sqrDecimator.reset();
		pulseDecimator.reset();
#endif
		phase = 0;	 
	} 

	static double log_freq_func (double w0, double w1, double indx)
	{	
		return pow (10.0, log10 (w0) + (log10 (w1) - log10 (w0)) * indx) ;
	}

	static double quad_freq_func (double w0, double w1, double indx)
	{	
		return w0 + (w1 - w0) * indx * indx ;
	}

	static double linear_freq_func (double w0, double w1, double indx)
	{	
		return w0 + (w1 - w0) * indx ;
	}

	float sin() {
		return sinBuffer[0];
		//return sinDecimator.process(sinBuffer);
	}
	float saw() {
		return sawBuffer[0];
		//return sawDecimator.process(sawBuffer);
	}

	float fmSaw() {
		return fmSawBuffer[0];	
	}
	
	float sqr() {
		return sqrBuffer[0];
		//return sqrDecimator.process(sqrBuffer);
	}
	float additive() {
		return additiveBuffer[0];	
	}

	float pulse() {
		return pulseBuffer[0];
		//return pulseDecimator.process(pulseBuffer);	
	}

	float light() {
		return sinf(2*M_PI * phase);
	}
};


struct KickBass {
	//Bezier::Bezier<3> *kickPitchBezier; 
	biquad *bq;
	KBVoltageControlledOscillator<KB_OVERSAMPLE, 16> oscillator;
	KBVoltageControlledOscillator<KB_OVERSAMPLE, 16> oscillator2;
	KBVoltageControlledOscillator<KB_OVERSAMPLE, 16> oscillator3;

	int ticks;
	int length = 19600;
	int isInitialized = 0;

	int note4;
	int note16;

	float blinkPhase = 0.f;
	float phase = -1;
	float sawPhase = -1;
	float sawFreq;
	float x1,y1,x2,y2;
	float kickPitchFreq;
	float kickFreqMax;
	float kickFreqMin;
	float filterQ;
	int kickRepeats;
	float bassFreqParam;
	int bassTicks = 0;
	float kickDecay = 1;
	int ticksPerClock = 0;
	int ticksPerClockRunner = 0;
	int playFromBuffer;
	int haveClockCycle = 0;
	int bar = 0;
	int noteOnTick = 0;
	int haveNoteOn = 0;
	float wavetable;
	float morph;
	int sample_rate = 44100;


	void Init() {
		//kickPitchBezier = new Bezier::Bezier<3>({ {0, 1}, {0.5, 0.5}, {0, 0}, {1, 0}});
		bq = bq_new(LOWPASS, 5000, 1, 1.0, sample_rate);
		bq_reset(bq);
	}

	void Destroy() {
		/*
		if (kickPitchBezier != NULL) {
			delete kickPitchBezier; 
			kickPitchBezier = NULL;
		}
		*/
		if (bq != NULL) {
			bq_destroy(bq);
			bq = NULL;
		}
	}

	float sawFunc(float pos)
	{
		return pos*2-1;
	}

	float lerp(float a, float b, float f) {
		return (a * (1.0 - f)) + (b * f);
	}

	float getKickPhase16() {
		if (note16 > 0)
			return 1;	
		float kickLength = length / 4.0f;
		return ticks / kickLength;
	}
	
	float getKickPhase12() {
		float kickLength = getKickLength12(); 
		float phase = ticks / kickLength;
		if (phase > 1)
			return 1;
		return phase;
	}

	float getKickPhaseTicks(int tick) {
		float kickLength = getKickLength12();
		float phase = tick / kickLength;
		if (phase > 1)
			return 1;
		return phase;
	}


	float getBassPhase() {
		float bassLength = length / 4.0f;
		return bassTicks / bassLength;

	}	
	// (60/BPM) * 44100 = samples per beat

	int getBPM() {
		float bpm = (60.0f / 4) / (ticksPerClock / 44100.0f); 
		return (int)floor(bpm);
	}

	int getBar() {
		return bar;	
	}

	char *getBassInfo() {
		static char bassInfo[64] = "";
		snprintf(bassInfo, sizeof(bassInfo), "%.0fhz %.1fQ",
				bassFreqParam,
				filterQ);
		bassInfo[20] = '\0';
		return bassInfo;
	}

	char *getKickInfo() {
		static char kickInfo[64] = "";
		snprintf(kickInfo, sizeof(kickInfo), "%.0fhz %.0fhz",
				floor(kickFreqMax),
				floor(kickFreqMin));
		kickInfo[20] = '\0';	
		return kickInfo;
	}

	int getSamples() {
		if (ticksPerClock < 5)
			ticksPerClock = 5;

		//return ticksPerClock * 2; // 8 notes
		// return ticksPerClock; // 4 notes
		return ticksPerClock / 4;
	}

	float getKickLength12() {
		return (length * 4) / 12.0f;	
	}

	void getKickPitchGraph(float u, point2 *out) {
		u = CLAMP(u, 0.0f, 1.0f);
		out->x = 3 * u * pow(1 - u, 2) * x1 + 3 * pow(u, 2) * (1 - u) * x2 + pow(u, 3);
		out->y = pow(1 - u, 3) + 3 * u * pow(1 - u, 2) * y1 + 3 * pow(u, 2) * (1 - u) * y2;
	} 

	float map(float s, float a1, float a2, float b1, float b2)
	{
		return b1 + (s-a1)*(b2-b1)/(a2-a1);
	}

	// 0 to 1
	float getBassGraph(float t) {
		t = CLAMP(t, 0.0f, 1.0f);
		float c;
		if (wavetable >= 0.5f) {
			c = oscillator2.getChirp(t, morph);
		}
		else {
			c = oscillator2.getSaw(t);
		}
		return map(c, -1.0f, 1.0f, 0.0f, 1.0f);
	}

	void bezierValueAt(float x[] , float y[], float t, point2 *out)
	{
		out->x = 3 * t * pow(1 - t, 2) * x[1] + 3 * pow(t, 2) * (1 - t) * x[2] + pow(t, 3);
		out->y = pow(1 - t, 3) + 3 * t * pow(1 - t, 2) * y[1] + 3 * pow(t, 2) * (1 - t) * y[2];
	}

	float getKickPitchRealtimeFast(float xTarget) {
		float tolerance = 0.00001f;
		float lower = 0.0f;
		float upper = 1.0f;
		float u = (upper + lower) / 2.0f;

		float x = 3 * u * pow(1 - u, 2) * x1 + 3 * pow(u, 2) * (1 - u) * x2 + pow(u, 3);

		int maxCount = 10;
		int count = 0;
		while (abs(xTarget - x) > tolerance && count < maxCount) {
			if (xTarget > x)
				lower = u;
			else 
				upper = u;

			u = (upper + lower) / 2.0f;
			x = 3 * u * pow(1 - u, 2) * x1 + 3 * pow(u, 2) * (1 - u) * x2 + pow(u, 3);
			count++;
		} 
		// y value at xTarget
		return pow(1 - u, 3) + 3 * u * pow(1 - u, 2) * y1 + 3 * pow(u, 2) * (1 - u) * y2;
	}

	float kickenv[13] = { 1, 0.5f, 1, 1,
				1,1,1,1,
				1, 0.75f, 0.5f, 0.25f, 0};

	float getKickEnvelope12(float t) {
		int len = 12; // 13 - 1
		int idx = floor(t * len);
		float from = kickenv[idx];
		float to = kickenv[idx + 1];
		float fraction =  t * len - idx;
		return lerp(from, to, fraction);
	}

	// x = 0 to 1
	// k = -1 to 1.0f
	float getSigmoid(float x, float k) {
		x = clamp(x, 0.0f, 1.0f);
		k = clamp(k, -1.0f, 1.0f);
		float a = k - x * 2.0f * k + 1.0f;
		float b = x - x * k; 
		return b / a; 
	}

	enum {
		CLK_OUT,
		KICK_OUT,
		BASS_OUT,
		LFO_OUT,
		NUM_OUTS
	};

	float outputs[NUM_OUTS];
	bool gateTrigger;
	const float FREQ_C4 = 261.6256f;
	
	void process(float sample_time, float _sample_rate, bool clk, bool rst, float _x1, float _y1, float _x2, float _y2, 
			float pitchVoltage, float freq, float resParam, float wavetableParam, float kickPitchMax, float bassVelocity) {
		x1 = _x1 * 0.25f;
		y1 = _y1 * 0.5f;
		x2 = _x2;
		y2 = _y2;
		gateTrigger = false;
		sample_rate = floor(_sample_rate);
		wavetable = wavetableParam; 
		bool useMorph = false;
		if (wavetable >= 0.5f) {
			morph = (wavetableParam - 0.5f) * 2.0f; 
			useMorph = true;
		}
		oscillator.wavetable = wavetableParam;
		oscillator2.morph = morph;

		if (rst) {
			noteOnTick = 0;
			haveNoteOn = 1;
		}

		if (clk) {
			ticksPerClock = ticksPerClockRunner;
			if (ticksPerClockRunner > 1000) {
				haveClockCycle = 1;
			}
			ticksPerClockRunner = 0;
			ticks = 0;
			note4 = 0;
			note16 = 0;
		}
		else {
			ticksPerClockRunner++;
		}

		if (haveClockCycle == 0) {
			outputs[KICK_OUT] = 0;
			outputs[BASS_OUT] = 0;
			return;
		}

		length = getSamples();

		if (ticks == 0 && note4 == 0) {
			gateTrigger = true;
			kickRepeats = 0;
		}

		pitchVoltage = CLAMP(pitchVoltage, -4.f, 4.f);

		float octave = -5;

		outputs[LFO_OUT] = (bar == 15) * 8.0f;

		float kickPhase = getKickPhase12();
		if (haveNoteOn) {
			kickPhase = getKickPhaseTicks(noteOnTick);
			outputs[BASS_OUT] = 0;
		}

		if (kickPhase <= 1.0f) {
			octave = -4;
			float kickAmp = getKickEnvelope12(kickPhase);
			float kickVoltageMin = pitchVoltage;
			float kickVoltageMax = kickVoltageMin + 2 + kickPitchMax * 8.0f;
			kickVoltageMax = CLAMP(kickVoltageMax, -8.0f, 10.0f);

			float kickPitch = getKickPitchRealtimeFast(kickPhase);
			float kickVoltageLerp = lerp(kickVoltageMin, kickVoltageMax, kickPitch);

			// for display
			kickFreqMax = oscillator.getPitchFreq(octave, kickVoltageMax, 0, 0);
			kickFreqMin = oscillator.getPitchFreq(octave, kickVoltageMin, 0, 0);

			if (ticks == 0) {
				oscillator.resetPhase();
			}

			oscillator.setPitchQ(octave, kickVoltageLerp, 0, 0);
			oscillator.process(sample_time);
			outputs[KICK_OUT] = 5.f * oscillator.sin() * kickAmp;
		}
		else {
			oscillator.resetPhase();
			outputs[KICK_OUT] = 0;
		}
	
		if (note16 > 0 && haveNoteOn == 0) {
			float bassVel = 1;
			float bassOctave = -4;
			float bassPitchCV = 0;
			float bassPitch = pitchVoltage;

			float cutoffPhase = 1 - getBassPhase();
			bassFreqParam = (freq * 5000);
			float logfreq = 20 + bassFreqParam * cutoffPhase;
			float filterRes = CLAMP(resParam, 0.f, 1.f);
			filterQ = powf(filterRes, 2) * 10.0f + 0.01f;

			if (note16 == 1) {	
				oscillator2.setPitchQ(bassOctave, bassPitch, bassPitchCV, 0);
				bassVel = bassVelocity;
			}
			else if (note16 == 2) {	
				oscillator2.setPitchQ(bassOctave, bassPitch, bassPitchCV, 0);
			}
			else if (note16 == 3) {	
				oscillator2.setPitchQ(bassOctave, bassPitch, bassPitchCV, 0);
			}
			oscillator2.wavetable = wavetable;

			float sigmoidVel = 1.0f - getSigmoid(getBassPhase(), 0.9f); 
			float input = 0;
			float out = 0;
			if (useMorph) {
				oscillator2.processPulse(sample_time);
				input = oscillator2.pulse();
				bq_update(bq, LOWPASS,logfreq, filterQ, 1.0, sample_rate);
				out = bq_process(bq, input) * 5.0f * bassVel * sigmoidVel;
				//oscillator2.processAdditive(sample_time, cutoffPhase * freq * 32.0f, powf(filterRes, 2));
				//out = oscillator2.additive() * 5.0f * bassVel;
			}
			else {
				//float fmMod = filterRes;
				//oscillator2.processFmSaw(sample_time, cutoffPhase, 0);
				//oscillator2.processLerp(sample_time, cutoffPhase);
				//out = input = oscillator2.fmSaw() * 5.0f * bassVel * cutoffPhase;
				//
				//oscillator2.processAdditive(sample_time, cutoffPhase * freq * 32.0f, powf(filterRes, 2));
				oscillator2.processSaw(sample_time);
				input = oscillator2.saw();
				//input = out = oscillator2.additive() * 5.0f * bassVel * cutoffPhase;
				bq_update(bq, LOWPASS, logfreq, filterQ, 1.0, sample_rate);
				out = bq_process(bq, input) * 5.0f * bassVel * sigmoidVel;
			}

			/*
			float clickLength = 500;
			if (bassTicks < clickLength) {
				float clickPhase = bassTicks / clickLength;
				float attackPhase = bassTicks / 20.0f;
				float clickVel = 1.0f;
				if (attackPhase < 1.0f) {
					clickVel = attackPhase;
				}

				float c = oscillator2.getClick(clickPhase, 2.0f, 50.0f, 1.0f);
				float clickOut = c * 5.0f * bassVel * clickVel;
				//outputs[BASS_OUT] = lerp(clickOut, out, clickPhase);
				input = lerp(clickOut, input, clickPhase);
			}
			*/

			outputs[BASS_OUT] = out;
		}
		else {
			outputs[BASS_OUT] = 0;
		}
	}

	void postProcess() {

		isInitialized = 1;

		ticks++;
		bassTicks++;
		noteOnTick++;
		
		if ((ticks % (length/4)) == 0) {
			note16++;
			if (note16 >= 4) {
				note4++;
				if (note4 >= 4) {
					bar++;
					if (bar >= 16) {
						bar = 0;
					}
					oscillator.resetPhase();
					note4 = 0;
				}
				if ((note4 % 2) == 0) {
					noteOnTick = 0;
				}
				ticks = 0;
				note16 = 0;
				haveNoteOn = 0;
			}
			bassTicks = 0;
			phase = -0.5f;
			sawPhase = -1;

			oscillator2.resetPhase();
			if (bq != NULL) {
				bq_reset(bq);
			}
		}
	}
};

#endif
