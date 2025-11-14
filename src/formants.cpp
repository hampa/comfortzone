#include "plugin.hpp"
#include <math.h>

#define pi 3.14159265359

#define LERP(a, b, f) (a * (1.0f - f)) + (b * f)


struct Formants : Module {
	enum ParamIds {
		PHONEME_PARAM,
		SAVE_PARAM,
		SPEAK_PARAM,
		F1_FREQ,
		F1_MOVE,
		F1_AMP,

		F2_FREQ,
		F2_MOVE,
		F2_AMP,

		F3_FREQ,
		F3_MOVE,
		F3_AMP,

		NOISE_PARAM,

		ATTACK_PARAM,
		DECAY_PARAM,

		NUM_PARAMS
	};
	enum InputIds {
		VOICE_INPUT,
		TRIG_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		FM_OUTPUT,
		AM_OUTPUT,
		VOICE_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		LIGHT0_LIGHT,
		LIGHT1_LIGHT,
		NUM_LIGHTS
	};
	


	/*
	const char *phonemes[] = {
		"aa",
		"ae",
		"ah",
		"ao",
		"aw",
		"ax",
		"ay",
		"b",
		"ch",
		"d",
		"dh",
		"eh",
		"er",
		"ey",
		"f",
		"g",
		"hh",
		"ih",
		"iy",
		"jh",
		"k",
		"l",
		"m",
		"n",
		"ng",
		"ow",
		"oy",
		"p",
		"r",
		"s",
		"sh",
		"t",
		"th",
		"uh",
		"uw",
		"v",
		"w",
		"y",
		"z",
		"zh"
	};
	*/
	struct formant_t {
		int idx;
		int type; // 1=plosive
		const char *name;
		float f1_freq;
		float f1_move;
		float f1_amp;

		float f2_freq;
		float f2_move;
		float f2_amp;

		float f3_freq;
		float f3_move;
		float f3_amp;

		float noise;

		float attack;
		float decay;
	}; 

	enum {
		PHONEME_VOWEL,
		PHONEME_PULSE,
		PHONEME_NOISE
	};

	#define NUM_PHONEMES 40
	formant_t formant_db[NUM_PHONEMES] = {
{ 0, 0, "aa", 0.16, 0.00, 0.62, 0.55,0.00,1.00, 0.70,0.00,0.55, 0.00, 0.00, 0.00 },
{ 1, 0, "ae", 0.17, 0.00, 0.52, 0.59,0.00,0.66, 0.79,0.00,0.20, 0.00, 0.00, 0.00 },
{ 2, 0, "ah", 0.17, 0.00, 0.42, 0.46,0.00,0.85, 0.76,0.00,0.12, 0.00, 0.00, 0.00 },
{ 3, 0, "ao", 0.19, 0.00, 0.44, 0.49,0.03,1.00, 0.65,0.00,0.45, 0.00, 0.00, 0.00 },
{ 4, 0, "aw", 0.15, 0.00, 0.35, 0.54,0.00,0.84, 0.69,0.00,0.25, 0.00, 0.00, 0.00 },
{ 5, 0, "ax", 0.15, 0.00, 0.36, 0.46,0.00,0.58, 0.76,0.00,0.04, 0.00, 0.00, 0.00 },
{ 6, 0, "ay", 0.17, 0.00, 0.45, 0.54,0.00,0.68, 0.80,0.00,0.19, 0.00, 0.00, 0.00 },
{ 7, 1, "b", 0.00, 0.00, 0.00, 0.00,0.00,0.00, 0.00,0.00,0.00, 0.00, 0.00, 0.00 },
{ 8, 2, "ch", 0.00, 0.00, 0.00, 0.00,0.00,0.00, 0.00,0.00,0.00, 0.00, 0.00, 0.00 },
{ 9, 1, "d", 0.00, 0.00, 0.00, 0.00,0.00,0.00, 0.00,0.00,0.00, 0.00, 0.00, 0.00 },
{ 10, 0, "dh", 0.00, 0.00, 0.00, 0.00,0.00,0.00, 0.00,0.00,0.00, 0.00, 0.00, 0.00 },
{ 11, 0, "eh", 0.00, 0.00, 0.00, 0.00,0.00,0.00, 0.00,0.00,0.00, 0.00, 0.00, 0.00 },
{ 12, 0, "er", 0.00, 0.00, 0.00, 0.00,0.00,0.00, 0.00,0.00,0.00, 0.00, 0.00, 0.00 },
{ 13, 0, "ey", 0.00, 0.00, 0.00, 0.00,0.00,0.00, 0.00,0.00,0.00, 0.00, 0.00, 0.00 },
{ 14, 1, "f", 0.00, 0.00, 0.00, 0.00,0.00,0.00, 0.00,0.00,0.00, 0.00, 0.00, 0.00 },
{ 15, 1, "g", 0.00, 0.00, 0.00, 0.00,0.00,0.00, 0.00,0.00,0.00, 0.00, 0.00, 0.00 },
{ 16, 0, "hh", 0.00, 0.00, 0.00, 0.00,0.00,0.00, 0.00,0.00,0.00, 0.00, 0.00, 0.00 },
{ 17, 0, "ih", 0.00, 0.00, 0.00, 0.00,0.00,0.00, 0.00,0.00,0.00, 0.00, 0.00, 0.00 },
{ 18, 0, "iy", 0.00, 0.00, 0.00, 0.00,0.00,0.00, 0.00,0.00,0.00, 0.00, 0.00, 0.00 },
{ 19, 0, "jh", 0.00, 0.00, 0.00, 0.00,0.00,0.00, 0.00,0.00,0.00, 0.00, 0.00, 0.00 },
{ 20, 1, "k", 0.01, 0.00, 1.00, 0.60,0.00,1.00, 0.32,0.00,1.00, 1.00, 0.00, 0.22 },
{ 21, 0, "l", 0.00, 0.00, 0.00, 0.00,0.00,0.00, 0.00,0.00,0.00, 0.00, 0.00, 0.00 },
{ 22, 0, "m", 0.17, 0.00, 0.71, 0.38,0.00,0.21, 0.69,0.00,0.03, 0.00, 0.00, 0.00 },
{ 23, 0, "n", 0.19, 0.00, 0.62, 0.37,0.00,0.21, 0.51,0.00,0.04, 0.00, 0.00, 0.00 },
{ 24, 0, "ng", 0.00, 0.00, 0.00, 0.00,0.00,0.00, 0.00,0.00,0.00, 0.00, 0.00, 0.00 },
{ 25, 0, "ow", 0.00, 0.00, 0.00, 0.00,0.00,0.00, 0.00,0.00,0.00, 0.00, 0.00, 0.00 },
{ 26, 0, "oy", 0.00, 0.00, 0.00, 0.00,0.00,0.00, 0.00,0.00,0.00, 0.00, 0.00, 0.00 },
{ 27, 1, "p", 0.00, 0.00, 0.00, 0.00,0.00,0.00, 0.00,0.00,0.00, 0.00, 0.00, 0.00 },
{ 28, 2, "r", 0.00, 0.00, 0.00, 0.00,0.00,0.00, 0.00,0.00,0.00, 0.00, 0.00, 0.00 },
{ 29, 2, "s", 0.87, 0.00, 1.00, 0.50,0.00,1.00, 0.79,0.00,1.00, 1.00, 0.00, 0.00 },
{ 30, 2, "sh", 0.29, 0.00, 0.71, 0.41,0.00,0.81, 0.33,0.00,0.72, 0.93, 0.00, 0.00 },
{ 31, 1, "t", -0.46, 0.00, 1.00, 0.00,0.00,1.00, 0.01,0.00,1.00, 0.91, 0.00, 0.04 },
{ 32, 1, "th", 0.00, 0.00, 0.00, 0.00,0.00,0.00, 0.00,0.00,0.00, 0.00, 0.00, 0.00 },
{ 33, 0, "uh", 0.00, 0.00, 0.00, 0.00,0.00,0.00, 0.00,0.00,0.00, 0.00, 0.00, 0.00 },
{ 34, 0, "uw", 0.00, 0.00, 0.00, 0.00,0.00,0.00, 0.00,0.00,0.00, 0.00, 0.00, 0.00 },
{ 35, 1, "v", 0.00, 0.00, 0.00, 0.00,0.00,0.00, 0.00,0.00,0.00, 0.00, 0.00, 0.00 },
{ 36, 1, "w", 0.05, 0.00, 0.50, 0.32,0.00,0.94, 0.50,0.00,0.15, 0.23, 0.00, 0.00 },
{ 37, 0, "y", 0.00, 0.00, 0.00, 0.00,0.00,0.00, 0.00,0.00,0.00, 0.00, 0.00, 0.00 },
{ 38, 2, "z", 0.00, 0.00, 0.00, 0.00,0.00,0.00, 0.00,0.00,0.00, 0.00, 0.00, 0.00 },
{ 39, 2, "zh", 0.00, 0.00, 0.00, 0.00,0.00,0.00, 0.00,0.00,0.00, 0.00, 0.00, 0.00 }
	};

	struct PhonemeQuantity : rack::engine::ParamQuantity {
		std::string getDisplayValueString() override {
			static const char* phonemes[] = {
				"aa", "ae", "ah", "ao", "aw", "ax", "ay", "b", "ch", "d",
				"dh", "eh", "er", "ey", "f", "g", "hh", "ih", "iy", "jh",
				"k", "l", "m", "n", "ng", "ow", "oy", "p", "r", "s", "sh",
				"t", "th", "uh", "uw", "v", "w", "y", "z", "zh"
			};

			int index = std::round(getValue());
			if (index < 0) {
				index = 0;
			}
			if (index >= (int)(sizeof(phonemes) / sizeof(phonemes[0]))) {
				index = (sizeof(phonemes) / sizeof(phonemes[0])) - 1;
			}

			return phonemes[index];
		}
	};

	struct FreqQuantity : rack::engine::ParamQuantity {
		float baseHz = 100.f; // 0 V = 100 Hz
		float getDisplayValue() override { return baseHz * std::pow(2.f, getValue()); }
		void setDisplayValue(float f) override { if (f > 0) setValue(std::log2(f / baseHz)); }
		std::string getDisplayValueString() override { return rack::string::f("%.1f", getDisplayValue()); }
		std::string getUnit() override { return "Hz"; }
	};


	Formants () {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		//configParam(F1_FREQ, 0.f, 1.f, 0.f, "f1_freq");
		//configParam(PHONEME, 0, 40, 0.f, "phoneme");
		configParam<PhonemeQuantity>(PHONEME_PARAM, 0.f, 39.f, 0.f, "Phoneme");
		configParam<FreqQuantity>(F1_FREQ, -3.f, 3.f, 0.f, "F1 frequency");
		configParam(SAVE_PARAM, 0, 1.f, 0.f, "save");
		configParam(SPEAK_PARAM, 0, 1.f, 0.f, "speak");

		configParam(F1_MOVE, -1.f, 1.f, 0.f, "f1_move");
		configParam(F1_AMP, 0.f, 1.f, 0.f, "f1_amp");
		configParam(F2_FREQ, 0.f, 1.f, 0.f, "f2_freq");
		configParam(F2_MOVE, -1.f, 1.f, 0.f, "f2_move");
		configParam(F2_AMP, 0.f, 1.f, 0.f, "f2_amp");

		configParam(F3_FREQ, 0.f, 1.f, 0.f, "f3_freq");
		configParam(F3_MOVE, -1.f, 1.f, 0.f, "f3_move");
		configParam(F3_AMP, 0.f, 1.f, 0.f, "f3_amp");

		configParam(NOISE_PARAM, 0.f, 1.f, 0.f, "noise");
		configParam(ATTACK_PARAM, 0.f, 1.f, 0.f, "attack");
		configParam(DECAY_PARAM, 0.f, 1.f, 0.f, "decay");
	}

	~Formants () {
	}


	double cv_offset(double freq) {
		if (freq <= 0) return 0.0; // avoid log of zero or negative
		return log2(freq / 100.0);
	}

	void setKnobValue(int paramId, float value) {
    		engine::ParamQuantity* pq = getParamQuantity(paramId);
    		if (pq) {
			pq->setValue(value);
		}
	}

	void loadPhoneme(int phoneme) {
		setKnobValue(F1_FREQ, formant_db[phoneme].f1_freq);
		setKnobValue(F2_FREQ, formant_db[phoneme].f2_freq);
		setKnobValue(F3_FREQ, formant_db[phoneme].f3_freq);

		setKnobValue(F1_AMP, formant_db[phoneme].f1_amp);
		setKnobValue(F2_AMP, formant_db[phoneme].f2_amp);
		setKnobValue(F3_AMP, formant_db[phoneme].f3_amp);

		setKnobValue(F1_MOVE, formant_db[phoneme].f1_move);
		setKnobValue(F2_MOVE, formant_db[phoneme].f2_move);
		setKnobValue(F3_MOVE, formant_db[phoneme].f3_move);

		setKnobValue(NOISE_PARAM, formant_db[phoneme].noise);
		setKnobValue(ATTACK_PARAM, formant_db[phoneme].attack);
		setKnobValue(DECAY_PARAM, formant_db[phoneme].decay);
	}
	
	void savePhoneme(int phoneme) {
		formant_db[phoneme].f1_freq = params[F1_FREQ].getValue();
		formant_db[phoneme].f2_freq = params[F2_FREQ].getValue();
		formant_db[phoneme].f3_freq = params[F3_FREQ].getValue();

		formant_db[phoneme].f1_amp = params[F1_AMP].getValue();
		formant_db[phoneme].f2_amp = params[F2_AMP].getValue();
		formant_db[phoneme].f3_amp = params[F3_AMP].getValue();

		formant_db[phoneme].f1_move = params[F1_MOVE].getValue();
		formant_db[phoneme].f2_move = params[F2_MOVE].getValue();
		formant_db[phoneme].f3_move = params[F3_MOVE].getValue();

		formant_db[phoneme].noise = params[NOISE_PARAM].getValue();
		formant_db[phoneme].attack = params[ATTACK_PARAM].getValue();
		formant_db[phoneme].decay = params[DECAY_PARAM].getValue();

		for (int i = 0; i < NUM_PHONEMES; i++) {
			DEBUG(":{ %i, %i, \"%s\", %.2f, %.2f, %.2f, %.2f,%.2f,%.2f, %.2f,%.2f,%.2f, %.2f, %.2f, %.2f },",
				i,
				formant_db[i].type,
				formant_db[i].name,
				formant_db[i].f1_freq, 
				formant_db[i].f1_move,
				formant_db[i].f1_amp,

				formant_db[i].f2_freq,
				formant_db[i].f2_move,
				formant_db[i].f2_amp,

				formant_db[i].f3_freq,
				formant_db[i].f3_move,
				formant_db[i].f3_amp,

				formant_db[i].noise,
				formant_db[i].attack,
				formant_db[i].decay
				);
		}
	}

	float phase = 0;
	float prev_trig_in = 0;
	float trig_phase = 0;
	int current_phoneme = 0;
	int prev_save_param = 0;

	float noise_amplitude = 1.0f;
	//float noise_amount = 0;

	float speak_time = 0;
	int phoneme_idx = 0;
#define SENTANCE_LENGTH 8
	int sentance[SENTANCE_LENGTH] = {20, 36, 0, 23, 31, 2, 22 };
	// QUANTUM  K W AA1 N T AH0 M
	// ASTRONAUTS  AE1 S T R AH0 N AO2 T S
	float phoneme_length = 0;

	void process(const ProcessArgs& args) override {
		float sample_time = args.sampleTime;
		float sample_rate = args.sampleRate;
		int phoneme = 0;
		bool got_trigger = false;

		if (params[SPEAK_PARAM].getValue() > 0) {
			speak_time += sample_time;
			if (speak_time > phoneme_length) {
				phoneme_idx++;
				if (phoneme_idx >= SENTANCE_LENGTH) {
					phoneme_idx = 0;
				}
				speak_time = 0;	
				got_trigger = true;
			}
			phoneme = sentance[phoneme_idx];
			if (formant_db[phoneme].type == 1) {
				phoneme_length = 0.1f;
			}
			else {
				phoneme_length = 0.2f;
			}
		}
		else {
			phoneme = (int)roundf(params[PHONEME_PARAM].getValue());
			float trig_in = inputs[TRIG_INPUT].getVoltage();
			if (trig_in > 0 && prev_trig_in == 0) {
				got_trigger = true;
			}
			prev_trig_in = trig_in;
		}

		if (current_phoneme != phoneme) {
			current_phoneme = phoneme;
			loadPhoneme(phoneme);
		}

		if (params[SAVE_PARAM].getValue() == 1 && prev_save_param == 0) {
			savePhoneme(phoneme);
		}
		prev_save_param = params[SAVE_PARAM].getValue();

		float f1_freq = params[F1_FREQ].getValue() * 5.0f;
		float f1_move = params[F1_MOVE].getValue();
		float f1_amp = params[F1_AMP].getValue();

		float f2_freq = params[F2_FREQ].getValue() * 5.0f;
		float f2_move = params[F2_MOVE].getValue();
		float f2_amp = params[F2_AMP].getValue();

		float f3_freq = params[F3_FREQ].getValue() * 5.0f;
		float f3_move = params[F3_MOVE].getValue();
		float f3_amp = params[F3_AMP].getValue();

		float noise_amount = params[NOISE_PARAM].getValue();
		float attack = params[ATTACK_PARAM].getValue();
		float decay = params[DECAY_PARAM].getValue();

		float out = inputs[VOICE_INPUT].getVoltage();

		float cv_out = 0;
		float amp_out = 0;

		if (got_trigger) {
			trig_phase = 1;
			if (formant_db[phoneme].type == 1) {
				noise_amplitude = 1.0f;
			}
		}

		if (phase < 0.01f) {
			cv_out = f1_freq + f1_move * trig_phase;
			amp_out = f1_amp;
		}
		else if (phase < 0.02f) {
			cv_out = f2_freq + f2_move * trig_phase;
			amp_out = f2_amp;
		}
		else if (phase < 0.03f) {
			cv_out = f3_freq + f3_move * trig_phase;
			amp_out = f3_amp;
		}
		else {
			phase = 0;
			cv_out = f1_freq;
			amp_out = f1_amp;
		}

		//if (noise_amplitude > 0 && noise_amount > 0) {
		if (formant_db[phoneme].type == 1) {
			float n = 2.0f * ((float)rand() / (float)RAND_MAX) - 1.0f;

			cv_out += n * noise_amount * 5;
			amp_out *= noise_amplitude; 

			noise_amplitude *= 1.0f - decay * 0.01f;
			if (noise_amplitude < 0.001f) {
				noise_amplitude = 0;
			}
			//DEBUG("decay %f", noise_amplitude);
		}
		else if (formant_db[phoneme].type == 2) {
			float n = 2.0f * ((float)rand() / (float)RAND_MAX) - 1.0f;
			cv_out += n * noise_amount * 5;
		}

		//outputs[VOICE_OUTPUT].setVoltage(out);
		outputs[VOICE_OUTPUT].setVoltage(noise_amplitude);
		outputs[FM_OUTPUT].setVoltage(cv_out);
		outputs[AM_OUTPUT].setVoltage(amp_out);

		phase += sample_time;
		trig_phase -= sample_time * 5;
		if (trig_phase < 0.0f) {
			trig_phase = 0;
		}
	}
};

struct FormantsWidget : ModuleWidget {
	Label* labelPitch;

	FormantsWidget(Formants* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dev10HP.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));

		float spacingX = box.size.x / (float)8.0f;
		float y = 65;
		int x0 = spacingX;
		//int x1 = box.size.x - spacingX;
		int x1 = spacingX * 3; //box.size.x - spacingX;
		int x2 = spacingX * 5; //box.size.x - spacingX;
		int x4 = box.size.x - spacingX;
		int xcenter = box.size.x / 2.0f;

		addParam(createParamCentered<VCVButton>(Vec(x0, y), module, Formants::SAVE_PARAM));
		addParam(createParamCentered<RoundHugeBlackKnob>(Vec(xcenter, y), module, Formants::PHONEME_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(Vec(x4, y), module, Formants::SPEAK_PARAM));

		y = 140;
		addParam(createParamCentered<RoundBlackKnob>(Vec(x0, y), module, Formants::F1_FREQ));
		addParam(createParamCentered<RoundBlackKnob>(Vec(x1, y), module, Formants::F1_MOVE));
		addParam(createParamCentered<RoundBlackKnob>(Vec(x2, y), module, Formants::F1_AMP));

		y = 190;
		addParam(createParamCentered<RoundBlackKnob>(Vec(x0, y), module, Formants::F2_FREQ));
		addParam(createParamCentered<RoundBlackKnob>(Vec(x1, y), module, Formants::F2_MOVE));
		addParam(createParamCentered<RoundBlackKnob>(Vec(x2, y), module, Formants::F2_AMP));

		//y = 272;
		y = 240;
		addParam(createParamCentered<RoundBlackKnob>(Vec(x0, y), module, Formants::F3_FREQ));
		addParam(createParamCentered<RoundBlackKnob>(Vec(x1, y), module, Formants::F3_MOVE));
		addParam(createParamCentered<RoundBlackKnob>(Vec(x2, y), module, Formants::F3_AMP));

		y = 290;
		addParam(createParamCentered<RoundBlackKnob>(Vec(x0, y), module, Formants::NOISE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(Vec(x1, y), module, Formants::ATTACK_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(Vec(x2, y), module, Formants::DECAY_PARAM));

		y = 310;
		addInput(createInputCentered<PJ301MPort>(Vec(x0, y), module, Formants::TRIG_INPUT));
		addInput(createInputCentered<PJ301MPort>(Vec(x1, y), module, Formants::VOICE_INPUT));

		y = 344;
		addOutput(createOutputCentered<PJ301MPort>(Vec(x0, y), module, Formants::FM_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(x1, y), module, Formants::VOICE_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(x2, y), module, Formants::AM_OUTPUT));
	}
};

Model* modelFormants = createModel<Formants, FormantsWidget>("formants");
