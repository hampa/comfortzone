#include "plugin.hpp"
#include "oscillator.h"
#include <math.h>

#define LERP(a, b, f) (a * (1.0f - f)) + (b * f)

struct Tzfmlead : Module {
	enum ParamIds {
		ROOT_NOTE_PARAM,
		FM_PARAM,
		MODULATOR_OCTAVE_PARAM,
		CARRIER_OCTAVE_PARAM,
		MODULATOR_WAVE_PARAM,
		CARRIER_WAVE_PARAM,
		STEREO_PARAM,
		NUM_PARAMS
	};

	enum InputIds {
		FM_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		MODULATOR_OUTPUT,
		CARRIER_OUTPUT,
		FM_LEFT_OUTPUT,
		FM_RIGHT_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		LIGHT0_LIGHT,
		LIGHT1_LIGHT,
		NUM_LIGHTS
	};

	Oscillator osc_carrier;
	Oscillator osc_modulator;
	Oscillator osc_fm_center;
	Oscillator osc_fm_left;
	Oscillator osc_fm_right;

	Tzfmlead () {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(ROOT_NOTE_PARAM, -12.f, 12.f, 0, "Root Note");
		configParam(FM_PARAM, 0.f, 1.f, 0.f, "FM Amount", "%", 0.f, 100.f);
		configParam(STEREO_PARAM, 0, 1.f, 0.f, "Stereo spread", "%", 0.f, 100.f);
		configParam(MODULATOR_OCTAVE_PARAM, 0.f, 8.f, 0.f, "Modulator Octave");
		configParam(CARRIER_OCTAVE_PARAM, -4.0f, 4.f, 0.f, "Carrier octave");
		configParam(MODULATOR_WAVE_PARAM, 0.f, Oscillator::WAVE_LAST - 1, 0.f, "Modulator Wave");
		configParam(CARRIER_WAVE_PARAM, 0, Oscillator::WAVE_LAST - 1, 0.f, "Carrier Wave");
		configInput(FM_INPUT, "FM Amount");
	}

	~Tzfmlead () {
	}

	const char *getNoteName(int i) {
		const char *notes[] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "G#", "A", "Bb", "B" };
		return notes[i % 12];
	}

	int fmNote = 0;
	const char *getRootNoteName() {
		return getNoteName(fmNote);
	}

	float phaseModulator = 0;
	float phaseCarrier = 0;
	float prev_center = 0;
	float prev_left = 0;

	void onSampleRateChange(const SampleRateChangeEvent& e) override {
		osc_modulator.Init(e.sampleRate);
		osc_carrier.Init(e.sampleRate);
		osc_fm_left.Init(e.sampleRate);
		osc_fm_center.Init(e.sampleRate);
		osc_fm_right.Init(e.sampleRate);

		osc_carrier.SetWaveform(Oscillator::WAVE_SAW);
		osc_modulator.SetWaveform(Oscillator::WAVE_SAW);
		osc_fm_left.SetWaveform(Oscillator::WAVE_SAW);
		osc_fm_center.SetWaveform(Oscillator::WAVE_SAW);
		osc_fm_right.SetWaveform(Oscillator::WAVE_SAW);
	}

	float lerp(float a, float b, float f) {
		return (a * (1.0 - f)) + (b * f);
	}

	float kbEucMod(float a, float b) {
		float mod = fmod(a, b);
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
		//float rootNoteParamOLD = params[ROOT_NOTE_PARAM].getValue();
		//float rootNoteParam = floorf((params[ROOT_NOTE_PARAM].getValue() * 12.0f)) / 12.0f;
		float rootNoteMidi = floorf((params[ROOT_NOTE_PARAM].getValue()));
		float stereo_param = params[STEREO_PARAM].getValue();
		float fmParam = params[FM_PARAM].getValue();

		int modulator_octave = floorf(params[MODULATOR_OCTAVE_PARAM].getValue());
		int carrier_octave = floorf(params[CARRIER_OCTAVE_PARAM].getValue());
		int modulator_wave = floorf(params[MODULATOR_WAVE_PARAM].getValue());
		int carrier_wave = floorf(params[CARRIER_WAVE_PARAM].getValue());

		fmNote = floorf(rootNoteMidi);

		float fm_input01 = 1.0f;
		if (inputs[FM_INPUT].isConnected()) {
			// 0 to 10 for an ADSR
			fm_input01 = inputs[FM_INPUT].getVoltage(0) * 0.1f;
		}

		osc_modulator.SetWaveform(modulator_wave);
		osc_carrier.SetWaveform(carrier_wave);

		osc_fm_left.SetWaveform(carrier_wave);
		osc_fm_center.SetWaveform(carrier_wave);
		osc_fm_right.SetWaveform(carrier_wave);

		//float root_pitch = 0; // midi 64
		//float pitch = carrier_octave * 12;
		float pitch = carrier_octave + (rootNoteMidi / 12.0f);
		//float root_freq = mtof(64);
		float modulator_freq = mtof(modulator_octave * 12.0f + rootNoteMidi);
		float carrier_freq = mtof(carrier_octave * 12.0f + rootNoteMidi);

		//float carrier_freq = mtof(64 + carrier_octave * 12);

		//float sample_time = args.sampleTime;
		//float sample_rate = args.sampleRate;

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

		/*
		float thing = dsp::FREQ_C4 * std::pow(2.f, pitch + 30.f) / std::pow(2.f, 30.f);
		float fm_freq = dsp::FREQ_C4 * std::pow(2.f, pitch + 30.f) / std::pow(2.f, 30.f);
		fm_freq += dsp::FREQ_C4 * fModulator * 5.0f * fmParam;
		osc_fm.SetFreq(fm_freq);
		*/
		//osc_fm_left.SetTZFM(pitch, fModulator, fmParam + 0.1f);
		osc_fm_left.SetTZFM(pitch, fModulator, fmParam * fm_input01, -stereo_param * 5.0f);
		osc_fm_center.SetTZFM(pitch, fModulator, fmParam * fm_input01, 0.0f);
		osc_fm_right.SetTZFM(pitch, fModulator, fmParam * fm_input01, stereo_param * 5.0f);

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

		float center = osc_fm_left.Process();
		//float left = prev_center;
		//float right = prev_left;
		float left = osc_fm_left.Process();
		float right = osc_fm_right.Process();
		float left_mix = LERP(center, left, 0.5f);
		float right_mix = LERP(center, right, 0.5f);

		//setFmFreq(20.0f + 200.0f * pitch, 0);
		outputs[MODULATOR_OUTPUT].setVoltage(fModulator * 5.0f);
		outputs[CARRIER_OUTPUT].setVoltage(fCarrier * 5.0f);
		outputs[FM_LEFT_OUTPUT].setVoltage(left_mix * 5.0f);
		outputs[FM_RIGHT_OUTPUT].setVoltage(right_mix * 5.0f);

		//prev_left = prev_center;
		//prev_center = center;


		// M = (L + R) / 2
		// S = (L - R) / 2 * width
		// L = M + S * width
		// R = M - S * width

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
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/tzfmlead10HP.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));

		float spacingX = box.size.x / (float)4.0f;
		int x0 = spacingX;
		int x1 = box.size.x - spacingX;

		float y = 72;
		addParam(createParamCentered<RoundBlackKnob>(Vec(x0, y), module, Tzfmlead::ROOT_NOTE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(Vec(x1, y), module, Tzfmlead::STEREO_PARAM));

		y = 142;
		addParam(createParamCentered<RoundBlackKnob>(Vec(x0, y), module, Tzfmlead::FM_PARAM));
		addInput(createInputCentered<PJ301MPort>(Vec(x1, y), module, Tzfmlead::FM_INPUT));

		y = 190;
		addParam(createParamCentered<RoundBlackKnob>(Vec(x0, y), module, Tzfmlead::MODULATOR_WAVE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(Vec(x1, y), module, Tzfmlead::CARRIER_WAVE_PARAM));

		y = 238;
		addParam(createParamCentered<RoundBlackKnob>(Vec(x0, y), module, Tzfmlead::MODULATOR_OCTAVE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(Vec(x1, y), module, Tzfmlead::CARRIER_OCTAVE_PARAM));

		y = 296;
		addOutput(createOutputCentered<PJ301MPort>(Vec(x0, y), module, Tzfmlead::MODULATOR_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(x1, y), module, Tzfmlead::CARRIER_OUTPUT));

		y = 344;
		addOutput(createOutputCentered<PJ301MPort>(Vec(x0, y), module, Tzfmlead::FM_LEFT_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(x1, y), module, Tzfmlead::FM_RIGHT_OUTPUT));

		labelPitch = createWidget<Label>(Vec(5, 100));
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
		Tzfmlead * module = dynamic_cast<Tzfmlead*>(this->module);
		if (module) {
			labelPitch->text = module->getRootNoteName();
		}
		ModuleWidget::step();
	}
};

Model* modelTzfmlead = createModel<Tzfmlead, TzfmleadWidget>("tzfmlead");
