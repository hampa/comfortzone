#ifndef KICKBASS_H
#define KICKBASS_H 1

#include "ripples.hpp"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

//static const int UPSAMPLE = 2;
const int KB_OVERSAMPLE = 1;

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

//#define WITH_DECIMATOR 1
#define CLAMP(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

//#define USE_SIGMOID 1

template <int OVERSAMPLE, int QUALITY>
struct KBVoltageControlledOscillator {
	float phase = 0.0f;
	float freq;
	float pw = 0.5f;
	float pitch;
	float voltage;
	//float wavetable = 0;
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
	float phaseOffset;

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
	float kbEucMod(float a, float b) {
		float mod = fmod(a, b);
		//float mod = fmod(a, b);
		if (mod < 0.f) {
			mod += b;
		}
		return mod;
	}
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

	float getFreqFromNote(float note) {
		return 261.626f * powf(2.0f, note / 12.0f); 
	}

	float getVoltageFromNote(float note) {
		return note / 12.0f;
	}

	float getBassFreqFromNote(float octave, float note, float voltage) {
		return 261.626f * powf(2.0f, octave + (note / 12.0f) + voltage);
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
		float sin_val = sinf(powf(_phase, 1.0f / M_PI) * M_PI * a);
		return sin_val;
	}

	float getClick(float _phase, float minFreq, float maxFreq, float _morph) {
		float a = lerp(minFreq, maxFreq, _morph);
		float sin_val = sinf(powf(_phase, 1.0f / M_PI) * M_PI * a);
		return sin_val;
	}

	float getChirpOLD(float w1, float w2, float time) {
		float res;
		time = sqrt(time);
		res = cos(w1 * time + (w2 - w1) * time * time / 2);

		//res = A * Mathf.Cos(w1 * time + (w2 - w1) * time / (2 * M));
		return res;
	}

	double getChirpRange(float w1, float w2, float M, float time, float sigmoid) {
   		float res;
		float curve = (time * time * time);
		//float curve = 1.0f - getSigmoid(time, sigmoid);
   		res=sinf(w1 * time + (w2 - w1) * curve / (2.0f * M_PI));
   		//res=sinf(2.f * M_PI *
   		return res;
	}

	float getChirpRange2(float t, float delta, float phase, float start, float end, float phi0) {
    		phase = 2.0f * M_PI * t * (start + (end - start) * delta / 2.0f);
    		return sinf(phase + phi0);
   	}

	int sawType = 0;
	float getSaw(float phase) {
		float factor;
		float phase2 = phase * 2.0f - 1.0f;
		switch (sawType) {
			case 0:
				factor = lerp(0, 1.4f, morph);
				return -(phase2 - (factor * sinf(phase2 * M_PI)));
			case 1:
				factor = lerp(1.0, 2.0f, morph);
				return -2.0f * (sinf((phase * factor) *M_PI * 0.5f) - 0.5f);
			case 2:
				return -sinf(morph * M_PI + ((phase * 2.0f - 1.0f)) * M_PI * 0.5f);
			case 3:
				factor = lerp(0.1f, 0.9f, morph);
				return sinf(M_PI * factor * factor * 32.0f * logf(phase + 0.0001f));
			case 4:
				return getChirp(phase, morph);
		}
		return lerp(1.0f, -1.0f, phase);
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
			float sine = sinf(2.f * M_PI * phase + feedbackAmount * feedbackSample);
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
			float sine = cosf(2.0f * M_PI * phase);
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

	float getSigmoid(float x, float k) {
		x = clamp(x, 0.0f, 1.0f);
		k = clamp(k, -1.0f, 1.0f);
		float a = k - x * 2.0f * k + 1.0f;
		float b = x - x * k; 
		return (b / a); 
	}

	void processChirp(float ticks, float startPitch, float endPitch, float t) {
		float d = ticks / 44100.0f;

		float startFreq = 261.626f * powf(2.0f, startPitch);
		float endFreq = 261.626f * powf(2.0f, endPitch);
		float freqAt = lerp(startFreq, endFreq, t);
		DEBUG("ticks %.2f t %.2f freqAt %.2f delta %.2f", ticks, t, freqAt, d);
		sinBuffer[0] = sinf(2.0f * M_PI * freqAt * d);
			
    		//phase = 2.0f * M_PI * t * (start + (end - start) * delta / 2.0f);
		/*
		float deltaPhase = ticks / 44100.0f
		float startFreq = 261.626f * powf(2.0f, startPitch);
		float endFreq = 261.626f * powf(2.0f, endPitch);
		float freq = lerp(startFreq, endFreq, t);
		float deltaPhase = CLAMP(freq * deltaTime, 1e-6, 0.5f);
		sinBuffer[0] = sinf(2.f * M_PI * thePhase);
		phase += deltaPhase;
		phase = kbEucMod(phase, 1.0f);
		*/
	}

	void processSin(float ticks, float startPitch, float endPitch, float t) {
		float startFreq = 261.626f * powf(2.0f, startPitch);
		float endFreq = 261.626f * powf(2.0f, endPitch);
		//float freqAt = lerp(startFreq, endFreq, t);

		sinBuffer[0] = sinf(2.f * M_PI * (ticks / 44100.0f) * startFreq);
		//sinBuffer[0] = sinf(2.f * M_PI * (ticks / 44100.0f) * freqAt);
		//float w1 = startFreq;
		//float w2 = endFreq;
		float delta = (ticks / 44100.f);
		float tt = 1 * delta; 
		//sinBuffer[0] = sinf(w1 * time + (w2 - w1) * time / (2.0f * M_PI));
		sinBuffer[0] = sinf(2.f * M_PI * tt * (startFreq + (endFreq - startFreq)) * delta / 2.0f);
	}

	void sweep(int ticks, float startPitch, float endPitch, float interval, float maxTicks) {
		float startFreq = 261.626f * powf(2.0f, startPitch);
		float endFreq = 261.626f * powf(2.0f, endPitch);
		float b = logf(endFreq/startFreq) / interval;
		float a = 2.0f * M_PI * startFreq / b;	

		float delta = ticks / maxTicks; 
		float t = interval * delta;
		float g_t = a * expf(b * t);
		sinBuffer[0] = sinf(g_t);
	}

	// sweep (16000, 16500, 256/44100.0f, 256);
	// 
	float processSweep(float phaseOffset, float f_start, float f_end, float interval, float n_steps, int i, int oversample, float factor) {
		if (factor < 1.0f)
			factor = 1.0f;
		//float factor = 2.0f;
    		float b = logf((f_end /f_start) * factor) / interval;
    		float a = 2.0f * M_PI * f_start / b;
		//DEBUG("i %i b %f a %f", i, b, a);

		//for (int i = 0; i < n_steps; i++) {
		for (int x = 0; x < oversample; x++) {
			float fractional_step = x / (float)oversample;
        		//float delta = i / float(n_steps);
        		float delta = (i + fractional_step) / float(n_steps);
        		float t = interval * delta;
        		float g_t = a * expf((b * t) * factor);
			float result = sinf(g_t + phaseOffset);
			sinBuffer[x] = result;
		}
		return sinBuffer[0];
	}

	float lastUsedPhase;
	// phase goes from 0 to 1
	// output -1 to 1
	void process(float deltaTime) {
		// Advance phase
		float deltaPhase = CLAMP(freq * deltaTime, 1e-6, 0.5f);

		//double instantaneous_w, current_phase;	

		for (int i = 0; i < KB_OVERSAMPLE; i++) {
			sinBuffer[i] = sinf(2.f*M_PI * phase);
			// assume sinebuffer[0]
			if (i == 0) {
				lastUsedPhase = phase;
			}

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
		phase = phaseOffset;	 
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

#ifdef WITH_DECIMATOR
	float sin() {
		return sinDecimator.process(sinBuffer);
	}
	float saw() {
		return sawDecimator.process(sawBuffer);
	}
	float fmSaw() {
		return sawDecimator.process(fmSawBuffer);
	}
	float sqr() {
		return sqrDecimator.process(sqrBuffer);
	}
	float additive() {
		return sawDecimator.process(additiveBuffer);
	}
	float pulse() {
		return pulseDecimator.process(pulseBuffer);	
	}
#else
	float sin() {
		return sinBuffer[0];
	}
	float saw() {
		return sawBuffer[0];
	}

	float fmSaw() {
		return fmSawBuffer[0];
	}
	float sqr() {
		return sqrBuffer[0];
	}
	float additive() {
		return additiveBuffer[0];	
	}

	float pulse() {
		return pulseBuffer[0];
	}
#endif

	float light() {
		return sinf(2*M_PI * phase);
	}
};


struct KickBass {
	ripples::RipplesEngine engine;
	KBVoltageControlledOscillator<KB_OVERSAMPLE, 16> oscillator;
	KBVoltageControlledOscillator<KB_OVERSAMPLE, 16> oscillator2;
	KBVoltageControlledOscillator<KB_OVERSAMPLE, 16> oscillator3;

	int ticks;
	int epochTicks;
	float length = 19600;
	int isInitialized = 0;
	float kickPhaseAtFirstBass;
	int note4;
	int note16;
	float phase_offset = 0;	
	int bassNote;
	float bassFreq;
	float blinkPhase = 0.f;
	float phase = -1;
	float sawPhase = -1;
	float sawFreq;
	float x1,y1,x2,y2;
	float kickPitchFreq;
	float kickFreqMax;
	float kickVoltageMax;
	float kickVoltageMin;
	int kickFreqMaxNote;
	float kickFreqMin;
	int kickFreqMinNote;
	float filterQ;
	int kickRepeats;
	float bassFreqParam;
	int bassTicks = 0;
	int barTicks;
	int lfoTicks = 0;
	float kickDecay = 1;
	int ticksPerClock = 0;
	int ticksPerClockRunner = 0;
	int playFromBuffer;
	int haveClockCycle = 0;
	int bar = 0;
	int noteOnTick = 0;
	int haveNoteOn = 0;
	float morph;
	int sample_rate = 44100;


	void Init() {
		engine.setSampleRate(APP->engine->getSampleRate());
	}

	void Destroy() {
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

	/*
	float getKickPhaseAtFirstBass() {
		float kickLength = getKickLength12(); 
		float x = getKickLength16Note();
		float phase = ticks / kickLength;
		if (phase > 1)
			return 1;
		return phase;
	}
	*/

	float get16To8NoteTrippletRatio() {
		return 0.25f / (1.0f / 3.0f);
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

	char *getBarInfo() {
		static char barInfo[64] = "";
		snprintf(barInfo, sizeof(barInfo), "%i %.2f %.2f",
				bar,
				outputs[LFO_OUT],
				outputs[START_OUT]);
		barInfo[20] = '\0';
		return barInfo;
	}

	const char *getNoteName(int i) {
		const char *notes[] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "G#", "A", "Bb", "B" };
		return notes[i % 12];
	}

	const char *getBassNoteName() {
		return getNoteName(bassNote);
	}

	const char *getKickNoteName() {
		return getNoteName(kickFreqMinNote);
	}

	const char *getKickSweepNoteName() {
		return getNoteName(kickFreqMaxNote);
	}

	char *getBassInfo() {
		static char bassInfo[64] = "";
		snprintf(bassInfo, sizeof(bassInfo), "%.2fhz %i %s",
				bassFreq,
				bassNote,
				getNoteName(bassNote));
		bassInfo[20] = '\0';
		return bassInfo;
	}

	char *getKickInfo() {
		static char kickInfo[64] = "";
		snprintf(kickInfo, sizeof(kickInfo), "%.0fhz %s %.0fhz %s %.2fP",
				floor(kickFreqMin),
				getNoteName(kickFreqMinNote),
				floor(kickFreqMax),
				getNoteName(kickFreqMaxNote),
				kickPhaseAtFirstBass);
		kickInfo[63] = '\0';	
		return kickInfo;
	}

	int getSamples() {
		if (ticksPerClock < 5)
			ticksPerClock = 5;

		//return ticksPerClock * 2; // 8 notes
		// return ticksPerClock; // 4 notes
		return ticksPerClock / 4;
	}


	// 1/8 NOTE T 
	float getKickLength12() {
		return (length * 4) / 12.0f;	
	}

	float getKickLength16Note() {
		return (length * 4) / 16.0f;	
	}

	void getKickPitchGraph(float u, point2 *out) {
		u = CLAMP(u, 0.0f, 1.0f);
#ifdef USE_SIGMOID
		out->x = u;
		out->y = 1.0f - getSigmoid(u, sigmoid); 
#else
		out->x = 3 * u * pow(1 - u, 2) * x1 + 3 * pow(u, 2) * (1 - u) * x2 + pow(u, 3);
		out->y = pow(1 - u, 3) + 3 * u * pow(1 - u, 2) * y1 + 3 * pow(u, 2) * (1 - u) * y2;
#endif
	} 

	float map(float s, float a1, float a2, float b1, float b2)
	{
		return b1 + (s-a1)*(b2-b1)/(a2-a1);
	}

	// 0 to 1
	float getBassGraph(float t) {
		t += oscillator2.phaseOffset;
		t = oscillator2.kbEucMod(t, 1.0f);
		t = CLAMP(t, 0.0f, 1.0f);
		float c = oscillator2.getSaw(t);
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
		return (b / a); 
	}

	enum {
		CLK_OUT,
		KICK_OUT,
		BASS_OUT,
		LFO_OUT,
		GATE_OUT,
		START_OUT,
		BASS_ENV_OUT,
		BASS_RAW_OUT,
		PITCH_ENV_OUT,
		PITCH_OUT,
		NUM_OUTS
	};

	float outputs[NUM_OUTS];
	bool gateTrigger;
	const float FREQ_C4 = 261.6256f;
	float sigmoid = 0;
	float kickDelta;
	int sawType;

	void process(float sample_time, float _sample_rate, bool clk, bool rst, float _x1, float _y1, float _x2, float _y2, 
			float pitchVoltageParam, float freqParam, float resParam, float sawParam, float morphParam, float kickPitchMinParam,
			float kickPitchMaxParam, float bassVelocity, float bassPitchParam, float bassPhaseParam) {
		x1 = _x1 * 0.25f;
		x1 = _x1 * 0.25f;
		y1 = _y1 * 0.5f;
		x2 = _x2;
		y2 = _y2;
		sigmoid = lerp(-0.99f, -0.5f, _x1);
		gateTrigger = false;
		sample_rate = floor(_sample_rate);
		//wavetable = wavetableParam; 
		sawType = floorf(sawParam * 4.0f);
		oscillator2.phaseOffset = bassPhaseParam;
		/*
		bool useMorph = false;
		if (wavetable >= 0.5f) {
			morph = (wavetableParam - 0.5f) * 2.0f; 
			useMorph = true;
		}
		oscillator.wavetable = wavetableParam;
		oscillator2.morph = morph;
		*/
		morph = morphParam;
		if (rst) {
			noteOnTick = 0;
			haveNoteOn = 1;
		}
		//int lenx = (int)floor(length/4.0f);
		//DEBUG("clk %i ticks %i ticksPerClockRunner %i lenx %i", clk, ticks, ticksPerClockRunner, lenx);
		if (clk) {
			if (ticksPerClock != ticksPerClockRunner) {
				DEBUG("clock updated diff %i", ticksPerClock - ticksPerClockRunner);
			}
			//DEBUG("epoch ticks %i", epochTicks);
			ticksPerClock = ticksPerClockRunner;
			if (ticksPerClockRunner > 1000) {
				haveClockCycle = 1;
			}
			ticksPerClockRunner = 0;
			ticks = 0;
			note4 = 0;
			note16 = 0;
			barTicks = 0;
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
			kickDelta = 0;
			oscillator2.resetPhase();
		}

		pitchVoltageParam = CLAMP(pitchVoltageParam, -4.f, 4.f);

		outputs[GATE_OUT] = (bar == 15);
		if (haveClockCycle) {
			outputs[LFO_OUT] = lfoTicks / (float)(ticksPerClock * 16.0f);
			float so = lfoTicks / (float)(ticksPerClock * 15.0f);
			outputs[START_OUT] = CLAMP(1.f - so, 0.f, 1.f); 
		}
		else {
			outputs[LFO_OUT] = 0;
			outputs[START_OUT] = 1.f;
		}

		float kickPhase = getKickPhase12();
		if (haveNoteOn) {
			kickPhase = getKickPhaseTicks(noteOnTick);
			outputs[BASS_OUT] = 0;
			kickDelta = 0;
		}
		outputs[BASS_ENV_OUT] = 0;
		outputs[BASS_RAW_OUT] = 0;
		outputs[PITCH_ENV_OUT] = 0;
		outputs[PITCH_OUT] = 0;

		if (kickPhase <= 1.0f) {
			float kickAmp = getKickEnvelope12(kickPhase);
			outputs[PITCH_ENV_OUT] = kickAmp;
			//float kickVoltageMin = floor(kickPitchMinParam / 12.0f);
			//float kickVoltageMax = 2.0f + floor(kickPitchMaxParam / 12.0f); 

			kickFreqMaxNote = floor(kickPitchMaxParam * 36.0f);
			kickFreqMinNote = floor(kickPitchMinParam * 12.0f);
			kickFreqMax = oscillator.getFreqFromNote(24.0f + kickFreqMaxNote);
			kickFreqMin = oscillator.getFreqFromNote(kickFreqMinNote - 36.0f);
			kickVoltageMax = oscillator.getVoltageFromNote(24.0f + kickFreqMaxNote); 
			kickVoltageMin = oscillator.getVoltageFromNote(kickFreqMinNote - 36.0f);


#ifdef USE_SIGMOID
			float kickPitch = 1.0f - getSigmoid(kickPhase, sigmoid);
#else
			float kickPitch = getKickPitchRealtimeFast(kickPhase);
#endif
			//kickPitch = 0.5f;
			float kickFreqLerp = lerp(kickFreqMin, kickFreqMax, kickPitch);
			float kickVoltageLerp = lerp(kickVoltageMin, kickVoltageMax, kickPitch);

			if (ticks == 0) {
				// phase start at 1?
				oscillator.resetPhase();
			}
			oscillator.freq = kickFreqLerp;
			outputs[PITCH_OUT] = kickVoltageLerp;
			oscillator.process(sample_time);

			oscillator3.setPitchQ(0, 0, 0, 0);

			outputs[KICK_OUT] = oscillator.sin() * kickAmp;

			kickDelta += sample_time;
		}
		else {
			oscillator.resetPhase();
			outputs[KICK_OUT] = 0;
		}
	
		outputs[BASS_ENV_OUT] = 1.0f;
		if (note16 > 0 && haveNoteOn == 0) {
			float bassVel = 1;

			float cutoffPhase = 1 - getBassPhase();
			bassFreqParam = (freqParam * 5000);
			float filterRes = CLAMP(resParam, 0.f, 1.f);
			filterQ = powf(filterRes, 2) * 10.0f + 0.01f;
			outputs[BASS_ENV_OUT] = cutoffPhase;

			bassNote = floor(bassPitchParam * 11.0f);
			bassFreq = oscillator2.getBassFreqFromNote(-3, bassNote, pitchVoltageParam);
			oscillator2.freq = bassFreq;
			oscillator2.sawType = sawType;

			if (note16 == 1) {
				bassVel = bassVelocity;
				if (bassTicks == 0) {
					kickPhaseAtFirstBass = oscillator.lastUsedPhase;
				}
			}

			float input = 0;
			float out = 0;
			oscillator2.sawType = sawType;
			oscillator2.morph = morph;
			oscillator2.processSaw(sample_time);
			input = oscillator2.saw();
			outputs[BASS_RAW_OUT] = input * bassVel;

			ripples::RipplesEngine::Frame frame;
			frame.res_knob = 0;
			frame.freq_knob = 0;
			frame.fm_knob = 1.0f;
			frame.gain_cv_present = false;

			float voltageInput = input * 5.0f * bassVel;

			frame.res_cv = 0; 
                        frame.freq_cv = cutoffPhase * 8.0f;
                        frame.fm_cv = 0;
                        frame.input = voltageInput; 
                        frame.gain_cv = 0;

                        engine.process(frame);
			out = frame.lp4;
			outputs[BASS_OUT] = out;
		}
		else {
			outputs[BASS_OUT] = 0;
			outputs[BASS_RAW_OUT] = 0;
		}
	}

	void postProcess() {
		isInitialized = 1;

		ticks++;
		bassTicks++;
		noteOnTick++;
		barTicks++;
		epochTicks++;
		ticksPerClockRunner++;

		if (ticksPerClockRunner > ticksPerClock) {
			//DEBUG("waiting for reset %i %i", ticksPerClockRunner, ticksPerClock);
			return;
		}
		lfoTicks++;

		int nextTick = floor(ticksPerClock / 16.0f);
		if (nextTick == 0)
			return;
		if ((barTicks % nextTick) == 0) {
			note16++;
			if (note16 >= 4) {
				note4++;
				if (note4 >= 4) {
					bar++;
					if (bar >= 16) {
						bar = 0;
						lfoTicks = 0;
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
		}
	}
};

#endif
