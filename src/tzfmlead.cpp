#include "plugin.hpp"
#include "filter.h"
#include "distortion.h"
#include "oscillator.h"
#include <math.h>

#define pi 3.14159265359

#define kFilterLP 0
#define kFilterBP  1
#define kFilterHP  2
#define LERP(a, b, f) (a * (1.0f - f)) + (b * f)

struct Tzfmlead : Module {
	enum ParamIds {
		FREQ_PARAM,
		FM_PARAM,
		MODULATOR_OCTAVE_PARAM,
		CARRIER_OCTAVE_PARAM,
		MODULATOR_WAVE_PARAM,
		CARRIER_WAVE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		PITCH_INPUT,
		FM_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		MODULATOR_OUTPUT,
		CARRIER_OUTPUT,
		FM_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		LIGHT0_LIGHT,
		LIGHT1_LIGHT,
		NUM_LIGHTS
	};

	Oscillator osc_carrier;
	Oscillator osc_modulator;
	Oscillator osc_fm;

	Tzfmlead () {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(FREQ_PARAM, -75.f, 75.f, 0.f, "Frequency", " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
		configParam(FM_PARAM, -1.f, 1.f, 0.f, "Frequency modulation", "%", 0.f, 100.f);
		configParam(MODULATOR_OCTAVE_PARAM, 0.f, 8.f, 0.f, "Modulator Octave");
		configParam(CARRIER_OCTAVE_PARAM, -4.0f, 4.f, 0.f, "Carrier octave");
		configParam(MODULATOR_WAVE_PARAM, 0.f, Oscillator::WAVE_LAST - 1, 0.f, "Modulator Wave");
		configParam(CARRIER_WAVE_PARAM, 0, Oscillator::WAVE_LAST - 1, 0.f, "Carrier Wave");

		configInput(PITCH_INPUT, "1V/octave pitch");
		configInput(FM_INPUT, "Frequency modulation");

		//osc_carrier.SetWaveform(Oscillator::WAVE_POLYBLEP_SAW);
		//osc_modulator.SetWaveform(Oscillator::WAVE_POLYBLEP_SAW);

		//osc_carrier.SetWaveform(Oscillator::WAVE_POLYBLEP_SAW);
		//osc_modulator.SetWaveform(Oscillator::WAVE_POLYBLEP_SAW);

	}

	~Tzfmlead () {
	}


	float phaseModulator = 0;
	float phaseCarrier = 0;

	void onSampleRateChange(const SampleRateChangeEvent& e) override {
		//DEBUG("XXX Setting sample rate %f", e.sampleRate);
		osc_modulator.Init(e.sampleRate);
		osc_carrier.Init(e.sampleRate);
		osc_fm.Init(e.sampleRate);

		osc_carrier.SetWaveform(Oscillator::WAVE_SAW);
		osc_modulator.SetWaveform(Oscillator::WAVE_SAW);
		osc_fm.SetWaveform(Oscillator::WAVE_SAW);

	}

	float lerp(float a, float b, float f) {
		return (a * (1.0 - f)) + (b * f);
	}

	float kbEucMod(float a, float b) {
		float mod = fmod(a, b);
		//float mod = fmod(a, b);
		if (mod < 0.f) {
			mod += b;
		}
		return mod;
	}

	inline float mtof(float m) {
		return powf(2, (m - 69.0f) / 12.0f) * 440.0f;
	}

	int idx = 0;
	void process(const ProcessArgs& args) override {
		float freqParam = params[FREQ_PARAM].getValue() / 12.0f;
		float fmParam = params[FM_PARAM].getValue();
		float freqRoot  = dsp::FREQ_C4 * std::pow(2.f, params[FREQ_PARAM].getValue());
		int modulator_octave = floorf(params[MODULATOR_OCTAVE_PARAM].getValue());
		int carrier_octave = floorf(params[CARRIER_OCTAVE_PARAM].getValue());
		int modulator_wave = floorf(params[MODULATOR_WAVE_PARAM].getValue());
		int carrier_wave = floorf(params[CARRIER_WAVE_PARAM].getValue());

		osc_modulator.SetWaveform(modulator_wave);
		osc_carrier.SetWaveform(carrier_wave);
		osc_fm.SetWaveform(carrier_wave);
		//osc_carrier.SetWaveform(carrier_wave);

		//float root_pitch = 0; // midi 64
		//float pitch = carrier_octave * 12;
		float pitch = carrier_octave + freqParam;
		//float root_freq = mtof(64);
		float modulator_freq = mtof(modulator_octave * 12);
		float carrier_freq = mtof(64 + pitch);

		//float carrier_freq = mtof(64 + carrier_octave * 12);

		float sample_time = args.sampleTime;
		float sample_rate = args.sampleRate;

		//float fL = lerp(1.0f, -1.0f, phaseL);
		//float fCarrierOLD = lerp(1.0f, -1.0f, phaseCarrier);
		//float fModulatorOLD = lerp(1.0f, -1.0f, phaseModulator);


		osc_modulator.SetFreq(modulator_freq);
		osc_carrier.SetFreq(carrier_freq);
		float fModulator = osc_modulator.Process();
		/*
		if (osc_modulator.IsEOC()) {
			osc_carrier.Reset();
		}
		*/
		float fCarrier = osc_carrier.Process();

		//float pitch = freqParam; // + inputs[PITCH_INPUT].getVoltage(0);
		// noise: 64 = oscA 0 oct
		// noise: 48 = oscA -1 oct
		// noise: 36 = oscA -2 oct
		// noise: 24 = oscA -3 oct

		// getvoltage input -5 to 5
		// fmparam -1 to 1
		// freqParam -4 to 4 // voltage per octave
                // modulator 48 pitch or 36 (more raspy)
                // fmParam 36 semitones (100 max)
                // carier +3 octave
                // modulator ramp down, triangle
		// std::pow(2.f, x)
		//float freq = dsp::FREQ_C4 * std::pow(2.f, pitch + 30.f) / std::pow(2.f, 30.f);
		//float fmParam = 1.0f;
		//freq += dsp::FREQ_C4 * f * fmParam;
		//float freq = dsp::FREQ_C4 * dsp::approxExp2_taylor5(pitch + 30.f) / std::pow(2.f, 30.f);
		//float freq = dsp::FREQ_C4 * std::pow(2.f, pitch + 30.f) / std::pow(2.f, 30.f);
		//freq += dsp::FREQ_C4 * inputs[FM_INPUT].getVoltage(0) * fmParam;
		/*
		if (!linear) {
			pitch += inputs[FM_INPUT].getPolyVoltageSimd<float_4>(c) * fmParam;
			freq = dsp::FREQ_C4 * dsp::approxExp2_taylor5(pitch + 30.f) / std::pow(2.f, 30.f);
		} else {
			freq = dsp::FREQ_C4 * dsp::approxExp2_taylor5(pitch + 30.f) / std::pow(2.f, 30.f);
			freq += dsp::FREQ_C4 * inputs[FM_INPUT].getPolyVoltageSimd<float_4>(c) * fmParam;
		}
		*/

		// FM signal = (Modulator * 2^BaseVoltage) - 1
		// The Terrorform uses 200 Hz/volt, so I need to multiply by an additional factor to scale it to 261.625.
		// FM signal = (Modulator * 261.625/200 * 2^BaseVoltage) - 1

		float thing = dsp::FREQ_C4 * std::pow(2.f, pitch + 30.f) / std::pow(2.f, 30.f);
		float fm_freq = dsp::FREQ_C4 * std::pow(2.f, pitch + 30.f) / std::pow(2.f, 30.f);
		fm_freq += dsp::FREQ_C4 * fModulator * 5.0f * fmParam;
		//fm_freq = fabs(fm_freq);
/*
		if (fm_freq > 20000 || fm_freq < 0) {
			DEBUG("pitch %f fModulator %f cf %f mf %f fm_freq %f thing %f phase %f", pitch, fModulator, carrier_freq, modulator_freq, fm_freq, thing, phase);
		}
*/
		//fm_freq += dsp::FREQ_C4 * (fModulator * 5.0f) * fmParam;

		//pitch += (fModulator * 5.0f) * fmParam;
		//float fm_freq = dsp::FREQ_C4 * std::pow(2.f, pitch + 30.f) / std::pow(2.f, 30.f);
		/*
		if ((idx % 100) == 0) {
			DEBUG("pitch %f mod_freq %f fm_freq %f", pitch, modulator_freq, fm_freq);
		}
		*/

		osc_fm.SetFreq(fm_freq);
		float fFM = osc_fm.Process();
		float phase = osc_fm.GetPhase();
		if (phase < 0 || phase > TWOPI_F) {
			DEBUG("pitch %f fModulator %f cf %f mf %f fm_freq %f thing %f phase %f inc %f",
				pitch, fModulator, carrier_freq, modulator_freq, fm_freq, thing, phase, osc_fm.GetPhaseInc());
		}

		//setFmFreq(20.0f + 200.0f * pitch, 0);
		outputs[MODULATOR_OUTPUT].setVoltage(fModulator * 5.0f);
		outputs[CARRIER_OUTPUT].setVoltage(fCarrier * 5.0f);
		outputs[FM_OUTPUT].setVoltage(fFM * 5.0f);

		//float deltaPhaseL = sample_time * dsp::FREQ_C4;
		//float deltaPhaseL = sample_time * freqRoot;
		//phaseL += deltaPhaseL;
		//phaseL = kbEucMod(phaseL, 1.0f);


		/*
		float deltaPhaseCarrier = sample_time * fm_freq;
		phaseCarrier += deltaPhaseCarrier;
		phaseCarrier = kbEucMod(phaseCarrier, 1.0f);

		float deltaPhaseModulator = sample_time * modulator_freq;
		phaseModulator += deltaPhaseModulator;
		phaseModulator = kbEucMod(phaseModulator, 1.0f);
		*/

		idx++;
	}
};

struct TzfmleadWidget : ModuleWidget {
	Label* labelPitch;

	TzfmleadWidget(Tzfmlead* module) {
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

		addParam(createParamCentered<RoundBlackKnob>(Vec(x0, y), module, Tzfmlead::FREQ_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(Vec(x1, y), module, Tzfmlead::FM_PARAM));

		y = 140;
		addParam(createParamCentered<RoundBlackKnob>(Vec(x0, y), module, Tzfmlead::MODULATOR_WAVE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(Vec(x1, y), module, Tzfmlead::CARRIER_WAVE_PARAM));

		y = 201;
		//addInput(createInputCentered<PJ301MPort>(Vec(x0, y), module, Tzfmlead::PITCH_INPUT));
		//addInput(createInputCentered<PJ301MPort>(Vec(x0, y), module, Tzfmlead::FM_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(x0, y), module, Tzfmlead::MODULATOR_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(x1, y), module, Tzfmlead::CARRIER_OUTPUT));

		y = 272;
		addParam(createParamCentered<RoundBlackKnob>(Vec(x0, y), module, Tzfmlead::MODULATOR_OCTAVE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(Vec(x1, y), module, Tzfmlead::CARRIER_OCTAVE_PARAM));
		//addInput(createInputCentered<PJ301MPort>(Vec(x0, y), module, Tzfmlead::LEFT_INPUT));
		//addInput(createInputCentered<PJ301MPort>(Vec(x1, y), module, Tzfmlead::REC_INPUT));
		//addInput(createInputCentered<PJ301MPort>(Vec(x1, y), module, Tzfmlead::RIGHT_INPUT));

		y = 344;
		addOutput(createOutputCentered<PJ301MPort>(Vec(x0, y), module, Tzfmlead::FM_OUTPUT));
		//addOutput(createOutputCentered<PJ301MPort>(Vec(x1, y), module, Tzfmlead::CARRIER_OUTPUT));

		//addOutput(createOutputCentered<PJ301MPort>(Vec(x0, y), module, Tzfmlead::LP_OUTPUT));
		//addOutput(createOutputCentered<PJ301MPort>(Vec(x1, y), module, Tzfmlead::BP_OUTPUT));
		//addOutput(createOutputCentered<PJ301MPort>(Vec(x2, y), module, Tzfmlead::HP_OUTPUT));

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

		/*
	void step() override {
		Fmlead * module = dynamic_cast<Fmlead*>(this->module);
		if (module) {
			labelPitch->text = module->getFmNoteName();
		}
		ModuleWidget::step();
	}
		*/

};

Model* modelTzfmlead = createModel<Tzfmlead, TzfmleadWidget>("tzfmlead");
