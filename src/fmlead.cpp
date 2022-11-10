#include "plugin.hpp"
#include "filter.h"
#include "distortion.h"
#include <math.h>

#define pi 3.14159265359

#define kFilterLP 0
#define kFilterBP  1
#define kFilterHP  2
#define LERP(a, b, f) (a * (1.0f - f)) + (b * f)

struct MultiFilter {
	float q;
	float freq;
	float smpRate;
	float hp = 0.0f, bp = 0.0f, lp = 0.0f, mem1 = 0.0f, mem2 = 0.0f;

	void reset() {
		hp = bp = lp = mem1 = mem2 = 0.0f;
	}

	void setParams(float freq, float q, float smpRate) {
		this->freq = freq;
		this->q = q;
		this->smpRate = smpRate;

	}

	void calcOutput(float sample) {
		float g = tanf(pi * freq / smpRate);
		float R = 1.0f / (2.0f*q);
		hp = (sample - (2.0f*R + g)*mem1 - mem2) / (1.0f + 2.0f * R * g + g * g);
		bp = g * hp + mem1;
		lp = g * bp + mem2;
		mem1 = g * hp + bp;
		mem2 = g * bp + lp;
	}

	float getType(int type) {
		if (type == kFilterLP)
			return lp;
		if (type == kFilterBP)
			return bp;
		return hp;
	}
};

struct Fmlead : Module {
	enum ParamIds {
		PITCH_PARAM,
		COMPRESS_PARAM,
		LP_PARAM,
		BP_PARAM,
		HP_PARAM,
		MIX_PARAM,
		DETUNE_PARAM,
		DISTMODE_PARAM,
		DISTDRIVE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		LEFT_INPUT,
		//RIGHT_INPUT,
		REC_INPUT,
		PITCH_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		LEFT_OUTPUT,
		RIGHT_OUTPUT,
		//LP_OUTPUT,
		//BP_OUTPUT,
		//HP_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		LIGHT0_LIGHT,
		LIGHT1_LIGHT,
		NUM_LIGHTS
	};

	#define STATE_DONE 0
	#define STATE_REC 1
	#define BUFFER_SIZE 2048
	float leftIdx = 0;
	float rightIdx = 0;
	Distortion *dist = NULL;

	Fmlead () {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(PITCH_PARAM, 0.f, 1.f, 0.f, "pitch");
		configParam(COMPRESS_PARAM, 0.f, 1.f, 0.f, "compress");
		configParam(LP_PARAM, 0.f, 1.f, 0.f, "lp");
		configParam(BP_PARAM, 0.f, 1.f, 0.f, "bp");
		configParam(HP_PARAM, 0.f, 1.f, 0.f, "hp");
		configParam(MIX_PARAM, 0.f, 1.f, 0.f, "mix");
		configParam(DETUNE_PARAM, 0.f, 1.f, 0.f, "detune");
		configParam(DISTMODE_PARAM, 0.f, 1.f, 0.f, "distmode");
		configParam(DISTDRIVE_PARAM, 0.f, 1.f, 0.f, "distdrive");

		for (int i = 0; i < BUFFER_SIZE; i++) {
			rawBuffer[i] = ssqBuffer[i] = leftBuffer[i] = rightBuffer[i] = 0;
		}
		createSSQ();
		createFilters(APP->engine->getSampleRate());
		setFmFreq(220.0f, 0.0f, APP->engine->getSampleRate());

		dist = new Distortion();
	}

	~Fmlead () {
		deleteFilters();
		if (dist != NULL) {
			delete dist;
		}
	}

	void createFilters(float sampleRate) {
		lpFilter = create_bw_low_pass_filter(4, sampleRate, 88);
		bpFilter = create_bw_band_pass_filter(8, sampleRate, 88, 2500);
		hpFilter = create_bw_high_pass_filter(4, sampleRate, 2500);
	}

	void deleteFilters() {
		if (lpFilter != NULL) {
			free_bw_low_pass(lpFilter);
		}
		if (bpFilter != NULL) {
			free_bw_band_pass(bpFilter);
		}
		if (hpFilter != NULL) {
			free_bw_high_pass(hpFilter);
		}
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override {
		//DEBUG("onSampleRateChange %f", e.sampleRate);
		deleteFilters();
		createFilters(e.sampleRate);
	}

	dsp::SchmittTrigger sampleTrig;
	int samples = 0;
	float leftSamples = 0;
	float rightSamples = 0;
	int fmLength = 0;
	int fmNote = 0;
	float leftBuffer[BUFFER_SIZE];
	float rightBuffer[BUFFER_SIZE];
	float ssqBuffer[BUFFER_SIZE];
	float rawBuffer[BUFFER_SIZE];
	int recState = 0;
	int recIdx = 0;
	MultiFilter filterLP, filterBP, filterHP;
	const int compressSize = 128;
	float LP[BUFFER_SIZE];
	float BP[BUFFER_SIZE];
	float HP[BUFFER_SIZE];
	BWLowPass *lpFilter = NULL;
	BWBandPass *bpFilter = NULL;
	BWHighPass *hpFilter = NULL;

	void multiCompress4(float sampleRate, float comp01, float lp, float bp, float hp) {
		//DEBUG("lp %f bp %f hp %f", lp, bp, hp);
		// 100, 500, 1000
		multiCompress3(sampleRate, kFilterLP, lp, comp01, &leftBuffer[0], &LP[0]);
		multiCompress3(sampleRate, kFilterBP, bp, comp01, &leftBuffer[0], &BP[0]);
		multiCompress3(sampleRate, kFilterHP, hp, comp01, &leftBuffer[0], &HP[0]);
		for (int i = 0; i < BUFFER_SIZE; i++) {
			//rightBuffer[i] = BP[i];
			rightBuffer[i] = (LP[i] * 0.9f + BP[i] * 1.1f + HP[i]) / 3.0f;
			//rightBuffer[i] = bw_band_pass(bpFilter, leftBuffer[i]);
			/*
			float f = LP[i];
			if (f > 8 || f < -8) {
				DEBUG("%i %f", i, f);
			}
			rightBuffer[i] = LP[i];
			*/
		}
	}
	// OT LP 100 BP 100 - 1000, HP 1000 +
	void multiCompress3(float sampleRate, int type, float cutoff, float comp01, float *in, float *out) {
		float q = 0.01f;
		int compressSize = comp01 * 512;
		if (compressSize < 4)
			compressSize = 4;

		float windows[BUFFER_SIZE];
		float samples[BUFFER_SIZE];
		int windowSize = BUFFER_SIZE / compressSize;
		float maxOut = 0;
		//filter->setParams(cutoff, q, sampleRate);
		for (int i = 0; i < BUFFER_SIZE; i++) {
			float sample = in[i] * 0.125f;
			//filter->calcOutput(sample);
			if (type == kFilterLP) {
				samples[i] = bw_low_pass(lpFilter, sample);
			}
			else if (type == kFilterBP) {
				samples[i] = bw_band_pass(bpFilter, sample);
			}
			else {
				samples[i] = bw_high_pass(hpFilter, sample);
			}
			//samples[i] = filter->getType(type);
			float x = fabs(samples[i]);
			if (x > maxOut) {
				maxOut = x;
			}
			// on the last sample
			if ((i % windowSize) == windowSize - 1) {
				int windowIdx = i / windowSize;
				if (maxOut > 0) {
					windows[windowIdx] = 1.0f / maxOut;
					//DEBUG("%i maxOut %f", windowIdx, maxOut);
				}
				else {
					windows[windowIdx] = 1.0f;
				}
				maxOut = 0;
			}
		}
		for (int i = 0; i < BUFFER_SIZE; i++) {
			int windowIdx = i / windowSize;
			out[i] = samples[i] * windows[windowIdx] * 8.0f;
			//printf("%i %f\n", i, out[i]);
		}
	}

	void multiCompress2(float samplesRate) {
		int compressSize = compress * 512;
		if (compressSize < 4)
			compressSize = 4;

		//float windows[compressSize];
		float windows[BUFFER_SIZE];
		int windowSize = BUFFER_SIZE / compressSize;
		float maxOut = 0;
		for (int i = 0; i < BUFFER_SIZE; i++) {
			float x = fabs(leftBuffer[i] * 0.125f);
			if (x > maxOut) {
				maxOut = x;
			}
			// on the last sample
			if ((i % windowSize) == windowSize - 1) {
				int windowIdx = i / windowSize;
				if (maxOut > 0) {
					windows[windowIdx] = 1.0f / maxOut;
					DEBUG("%i maxOut %f", windowIdx, maxOut);
				}
				else {
					windows[windowIdx] = 1.0f;
				}
				maxOut = 0;
			}
		}
		/*
		for (int i = 0; i < compressSize; i++) {
			DEBUG("%i %f", i, windows[i]);
		}
		*/
		for (int i = 0; i < BUFFER_SIZE; i++) {
			int windowIdx = i / windowSize;
			rightBuffer[i] = leftBuffer[i] * windows[windowIdx];
			// 431 LB 3.445618 mult 1.271287 RB 35.042969
			if (rightBuffer[i] > 8 || rightBuffer[i] < -8) {
				DEBUG("XX %i LB %f mult %f RB %f", i, leftBuffer[i], windows[windowIdx], rightBuffer[i]);
			}
		}
	}

	void multiCompress(float sampleRate) {
		float q = 0.1f;
		float bufferLP[BUFFER_SIZE];
		float bufferBP[BUFFER_SIZE];
		float bufferHP[BUFFER_SIZE];

		filterLP.setParams(800.0f, q, sampleRate);
		filterBP.setParams(1000.0f, q, sampleRate);
		filterHP.setParams(2000.0f, q, sampleRate);
		float outLP = 0;
		float outBP = 0;
		float outHP = 0;
		float maxLP = 0;
		float maxBP = 0;
		float maxHP = 0;
		float maxInput = 0;
		for (int i = 0; i < BUFFER_SIZE; i++) {
			float in = fabs(leftBuffer[i]) * 0.125f;
			if (in > maxInput) {
				maxInput = in;
			}
		}
		float inFactor = 1.0f;
		if (maxInput > 0.0f) {
			inFactor = 1.0f / maxInput;
		}
		DEBUG("max Input %f factor %f\n", maxInput, inFactor);

		for (int i = 0; i < BUFFER_SIZE; i++) {
			// convert to -1 to 1
			float in = leftBuffer[i] * 0.125f * inFactor;
			filterLP.calcOutput(in);
			filterBP.calcOutput(in);
			filterHP.calcOutput(in);
			outLP = filterLP.lp;
			outBP = filterBP.bp;
			outHP = filterBP.hp;
			bufferLP[i] = outLP;
			bufferBP[i] = outBP;
			bufferHP[i] = outHP;
			if (fabs(outLP) > maxLP) {
				maxLP = fabs(outLP);
			}
			if (fabs(outBP) > maxBP) {
				maxBP = fabs(outBP);
			}
			if (fabs(outHP) > maxHP) {
				maxHP = fabs(outHP);
			}
		}
		float factorLP = 1.0f;
		float factorBP = 1.0f;
		float factorHP  = 1.0f;
		if (maxLP > 0) {
			factorLP = 1.0f / maxLP;
		}
		if (maxBP > 0) {
			factorBP = 1.0f / maxBP;
		}
		if (maxHP > 0) {
			factorHP = 1.0f / maxHP;
		}

		for (int i = 0; i < BUFFER_SIZE; i++) {
			//rightBuffer[i] = ((bufferLP[i] * factorLP + bufferBP[i] * factorBP + bufferHP[i] * factorHP) / 3.0f) * 8.0f;
			rightBuffer[i] = bufferBP[i] * factorBP * 8.0f;
		}
	}

	void createSSQ() {
		float startFreq = 1;
		float endFreq = 4000;
		//float endFreq = 2000;
		float w0 = 2.0f * M_PI * startFreq / 44100.0f;
		float w1 = 2.0f * M_PI * endFreq / 44100.0f;

		float instantaneous_w = w0;  // starthz
		float current_phase = 0.0f;
		for (int i = 0; i < BUFFER_SIZE; i++) {
			float ph = (float)i / BUFFER_SIZE;
			rawBuffer[i] = leftBuffer[i] = rightBuffer[i] = ssqBuffer[i] = sinf(current_phase);
			float sweep = w0 + (w1 - w0) * ph;
			instantaneous_w = sweep;
			current_phase = fmod(current_phase + instantaneous_w, 2.0f * M_PI);
		}
	}

	void setFmFreq(float hz, float octave, float sampleRate) {
		if (hz > 0 && octave >= 0) {
			fmLength = floorf(sampleRate / (hz * (octave + 1.0f)));
		}
		int i = 0;
		while (fmLength >= BUFFER_SIZE && i < 5) {
			fmLength *= 0.5f;
			i++;

		}
	}

	const char *getNoteName(int i) {
		const char *notes[] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "G#", "A", "Bb", "B" };
		return notes[i % 12];
	}

	const char *getFmNoteName() {
		return getNoteName(fmNote);
	}

	float getFreqFromNote(float note) {
		// C 4
		//return 261.626f * powf(2.0f, note / 12.0f);
		// C 0
		return 16.352f * powf(2.0f, note / 12.0f);
	}

	float compress = 0;
	void process(const ProcessArgs& args) override {
		float sample_time = args.sampleTime;
		float sample_rate = args.sampleRate;

		float pitch = params[PITCH_PARAM].getValue();
		compress = params[COMPRESS_PARAM].getValue();
		float lp = LERP(50, 200, params[LP_PARAM].getValue());
		float bp = LERP(50, 5000, params[BP_PARAM].getValue());
		float hp = LERP(100, 5000, params[HP_PARAM].getValue());
		float mix = LERP(0.01f, 0.5f, params[MIX_PARAM].getValue());
		float detune = LERP(0, 0.1f, params[DETUNE_PARAM].getValue());

		float pitchCV = 0;
		fmNote = floorf(pitch * 36);
		float freq = getFreqFromNote(fmNote);
		if (inputs[PITCH_INPUT].isConnected()) {
			pitchCV = inputs[PITCH_INPUT].getVoltage();
			freq += powf(2.0f, pitchCV);
		}

		// std::pow(2.f, x)
		//freq = dsp::FREQ_C4 * dsp::approxExp2_taylor5(pitch + 30.f) / std::pow(2.f, 30.f);
		//freq += dsp::FREQ_C4 * inputs[FM_INPUT].getPolyVoltageSimd<float_4>(c) * fmParam;

		//setFmFreq(20.0f + 200.0f * pitch, 0);
		setFmFreq(freq, 0, args.sampleRate);

		dist->controls.mode = floorf(params[DISTMODE_PARAM].getValue() * 7);
		dist->controls.drive = 1.0f + params[DISTDRIVE_PARAM].getValue() * 5.0f;
		dist->controls.mix = 1.0f;


		bool doCapture = sampleTrig.process(inputs[REC_INPUT].getVoltage());
		if (doCapture && recState == STATE_DONE && inputs[LEFT_INPUT].isConnected()) {
			recState = STATE_REC;
			recIdx = 0;
			samples = 0;
			leftSamples = 0;
			rightSamples = 0;
		}
		if (recState == STATE_REC) {
			if (recIdx >= BUFFER_SIZE) {
				recState = STATE_DONE;
				recIdx = 0;
				multiCompress4(args.sampleRate, compress, lp, bp, hp);
			}
			else {
				if (inputs[LEFT_INPUT].isConnected()) {
					//float v = inputs[LEFT_INPUT].getVoltage() * 0.125f;
					//leftBuffer[recIdx] = dist->processSample(v) * 8.0f;
					leftBuffer[recIdx] = inputs[LEFT_INPUT].getVoltage();
					rawBuffer[recIdx] = inputs[LEFT_INPUT].getVoltage();
				}
				/*
				if (inputs[RIGHT_INPUT].isConnected()) {
					//rightBuffer[recIdx] = inputs[RIGHT_INPUT].getVoltage();
				}
				else if (inputs[LEFT_INPUT].isConnected()) {
					rightBuffer[recIdx] = leftBuffer[recIdx];
				}
				*/
				recIdx++;
			}
		}

		if (samples >= BUFFER_SIZE|| samples < 0) {
			samples = 0;
		}
		float center = rightBuffer[samples];
		int leftIdx = floorf(leftSamples);
		int rightIdx = floorf(rightSamples);
		float left = rightBuffer[leftIdx];
		float right = rightBuffer[rightIdx];

		float leftMix = LERP(center, left, mix);
		float rightMix = LERP(center, right, mix);

		//outputs[LEFT_OUTPUT].setVoltage(leftMix);
		//outputs[RIGHT_OUTPUT].setVoltage(rightMix);
		static int prevMode = -1;
		if (prevMode != dist->controls.mode) {
			DEBUG("%i\n", dist->controls.mode);
			prevMode = dist->controls.mode;
		}
		outputs[LEFT_OUTPUT].setVoltage(dist->processSample(leftMix * 0.125f) * 8.0f);
		outputs[RIGHT_OUTPUT].setVoltage(dist->processSample(rightMix * 0.125f) * 8.0f);

		//outputs[LEFT_OUTPUT].setVoltage(center);
		//outputs[RIGHT_OUTPUT].setVoltage(center);

		//outputs[FILTER_OUTPUT].setVoltage(bw_band_pass(bpFilter, inputs[LEFT_INPUT].getVoltage()));
		//outputs[LP_OUTPUT].setVoltage(bw_low_pass(lpFilter, inputs[LEFT_INPUT].getVoltage()));
		//outputs[BP_OUTPUT].setVoltage(bw_band_pass(bpFilter, inputs[LEFT_INPUT].getVoltage()));
		//outputs[HP_OUTPUT].setVoltage(bw_high_pass(hpFilter, inputs[LEFT_INPUT].getVoltage()));

		samples++;
		if (samples >= fmLength) {
                        samples = 0;
                }
		//DEBUG("%i %.2f %.2f", samples, leftSamples, rightSamples);
		leftSamples += 1.0f - detune * 0.1f;
		rightSamples += 1.0f + detune * 0.1f;
		if (leftSamples >= fmLength -20 || rightSamples >= fmLength -20) {
			leftSamples = rightSamples = 0;
		}
	}
};

struct FmleadWidget : ModuleWidget {
	Label* labelPitch;

	FmleadWidget(Fmlead* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/fmlead10HP.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		//addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));

		float spacingX = box.size.x / (float)4.0f;
		float y = 85;
		int x0 = spacingX;
		int x1 = box.size.x - spacingX;
		//int x2 = box.size.x / 2 + spacingX;
		//int x3 = box.size.x / 2 - spacingX;

		addParam(createParamCentered<RoundBlackKnob>(Vec(x0, y), module, Fmlead::COMPRESS_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(Vec(x1, y), module, Fmlead::DETUNE_PARAM));
		//addParam(createParamCentered<RoundBlackKnob>(Vec(x2, y), module, Fmlead::MIX_PARAM));

		y = 140;
		addParam(createParamCentered<RoundBlackKnob>(Vec(x0, y), module, Fmlead::DISTMODE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(Vec(x1, y), module, Fmlead::DISTDRIVE_PARAM));

		y = 201;
		addInput(createInputCentered<PJ301MPort>(Vec(x0, y), module, Fmlead::PITCH_INPUT));
		addParam(createParamCentered<RoundBlackKnob>(Vec(x1, y), module, Fmlead::PITCH_PARAM));

		y = 272;
		addInput(createInputCentered<PJ301MPort>(Vec(x0, y), module, Fmlead::LEFT_INPUT));
		addInput(createInputCentered<PJ301MPort>(Vec(x1, y), module, Fmlead::REC_INPUT));
		//addInput(createInputCentered<PJ301MPort>(Vec(x1, y), module, Fmlead::RIGHT_INPUT));

		y = 344;
		addOutput(createOutputCentered<PJ301MPort>(Vec(x0, y), module, Fmlead::LEFT_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(x1, y), module, Fmlead::RIGHT_OUTPUT));

		//addOutput(createOutputCentered<PJ301MPort>(Vec(x0, y), module, Fmlead::LP_OUTPUT));
		//addOutput(createOutputCentered<PJ301MPort>(Vec(x1, y), module, Fmlead::BP_OUTPUT));
		//addOutput(createOutputCentered<PJ301MPort>(Vec(x2, y), module, Fmlead::HP_OUTPUT));

		//labelPitch = createWidget<Label>(Vec(5, 216));
		labelPitch = createWidget<Label>(Vec(42, 216));
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
		Fmlead * module = dynamic_cast<Fmlead*>(this->module);
		if (module) {
			labelPitch->text = module->getFmNoteName();
		}
		ModuleWidget::step();
	}

};

Model* modelFmlead = createModel<Fmlead, FmleadWidget>("fmlead");
