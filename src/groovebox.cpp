#include "plugin.hpp"
#include "grooves.h"
#include "oscillator.h"
#include <math.h>

#define LERP(a, b, f) (a * (1.0f - f)) + (b * f)
#define LOG0001 -9.210340371976182f // -80dB
#define CLAMP(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))
#define NUM_PARAMS (16*NUM_INST+2)

struct GrooveBox : Module {
	enum ParamIds {
		//PATTERN_PARAM,
		ROOT_NOTE_PARAM
	};

	enum InputIds {
		CLOCK_INPUT,
		RESET_INPUT,
		NUM_INPUTS
	};

	enum OutputIds {
		GATE_MELODY_OUTPUT,
		MELODY_OUTPUT,
		GATE_BASS_OUTPUT,
		BASS_OUTPUT,
		GATE_KICK_OUTPUT,
		VEL_KICK_OUTPUT,
		GATE_SNARE_OUTPUT,
		VEL_SNARE_OUTPUT,
		GATE_CLOSED_HIHAT_OUTPUT,
		VEL_CLOSED_HIHAT_OUTPUT,
		GATE_OPEN_HIHAT_OUTPUT,
		VEL_OPEN_HIHAT_OUTPUT,
		NUM_OUTPUTS
	};

	enum LightIds {
		INVERT_LIGHT,
		LIGHT1_LIGHT,
		NUM_LIGHTS
	};

	GrooveBox () {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(ROOT_NOTE_PARAM, -12.0f, 12.f, 0, "Root Note", " Midi");

		for (int i = 0; i < NUM_PARAMS; i++) {
			configParam(i, 0, 1.0f, 0.0f, "k", "", 0.f, 127.0f);
		}

		configInput(CLOCK_INPUT, "Clock Input");
		configInput(RESET_INPUT, "Reset Input");
	}

	~GrooveBox () {
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override {
		osc_melody.Init(e.sampleRate);
		osc_bass.Init(e.sampleRate);
		osc_melody.SetWaveform(Oscillator::WAVE_SAW);
		osc_bass.SetWaveform(Oscillator::WAVE_SAW);
	}

	int current_step = 0;
	float prev_clock_input = -2;
	float prev_reset_input = -2;
	int gateon[NUM_INST];
	float amp[NUM_INST];
	float vel[NUM_INST];
	float bass_gateon = 0;
	float bass_amp = 0;
	float melody_gateon = 0;
	float melody_amp = 0;
	float bass_freq = 110;
	float melody_freq = 220;
	Oscillator osc_bass;
	Oscillator osc_melody;

	inline float fmap_linear(float in, float min, float max) {
		return CLAMP(min + in * (max - min), min, max);
	}

	inline float fmap_exp(float in, float min, float max) {
		return CLAMP(min + (in * in) * (max - min), min, max);
	}

	inline float fmap_log(float in, float min, float max) {
		const float a = 1.f / log10f(max / min);
		return CLAMP(min * powf(10, in / a), min, max);
        }

	inline float mtof(float m) {
		return powf(2, (m - 69.0f) / 12.0f) * 440.0f;
	}

	void process(const ProcessArgs& args) override {
		float clock_input = inputs[CLOCK_INPUT].getVoltage(0);
		float reset_input = inputs[RESET_INPUT].getVoltage(0);
		float root_note = floorf((params[ROOT_NOTE_PARAM].getValue())) + 21;

		params[0].setValue(0.5f);
		bool have_clock = false;
		if (prev_clock_input < 1 && clock_input > 1) {
			have_clock = true;
			current_step++;
			if (current_step >= 64) {
				current_step = 0;
			}
		}
		if (prev_reset_input < 1 && reset_input > 1) {
			current_step = 0;
		}

		if (have_clock) {
			for (int i = 0; i < NUM_INST; i++) {
				float v = grooves[i][current_step];
				if (v > 0) {
					gateon[i] = 30;
					if (v > 1) {
						v /= 127.0f;
					}	
					vel[i] = v * 5.0f;
				}
				else {
					gateon[i] = 0;
				}
			}
			if (grooves[INST_OPEN_HIHAT][current_step]) {
				gateon[INST_CLOSED_HIHAT] = 0;
			}
			if (grooves[INST_KICK][current_step]) {
				bass_gateon = 0;
			}
			else {
				bass_gateon = 30;
			}
			int track_idx = 2;
			int bass_note = bass_a[track_idx][current_step]; 
			int melody_note = melody_a[track_idx][current_step];
			int melody_vel = volume_a[track_idx][current_step];
			melody_freq = mtof(root_note + melody_note + 24); 
			bass_freq = mtof(root_note + bass_note + 12); 
			osc_melody.SetFreq(melody_freq);
			osc_bass.SetFreq(bass_freq);
			if (melody_vel > 0) {
				melody_gateon = 30;
			}
			else {
				melody_gateon = 0;
			}
		}

		for (int i = 0; i < NUM_INST; i++) {
			if (gateon[i] > 1) {
				gateon[i]--;
				amp[i] = 5;
			}
			else {
				gateon[i] = 0;
				amp[i] = 0;
			}
		}
		if (bass_gateon > 1) {
			bass_gateon--;
			bass_amp = 5;
		}
		else {
			bass_gateon = 0;
			bass_amp = 0;
		}

		if (melody_gateon > 1) {
			melody_gateon--;
			melody_amp = 5;
		}
		else {
			melody_gateon = 0;
			melody_amp = 0;
		}

		float melody = osc_melody.Process() * 5.0f;
		float bass = osc_bass.Process() * 5.0f;

		outputs[GATE_BASS_OUTPUT].setVoltage(bass_amp);
		outputs[BASS_OUTPUT].setVoltage(bass);

		outputs[GATE_MELODY_OUTPUT].setVoltage(melody_amp);
		outputs[MELODY_OUTPUT].setVoltage(melody);

		outputs[GATE_KICK_OUTPUT].setVoltage(amp[INST_KICK]);
		outputs[GATE_SNARE_OUTPUT].setVoltage(amp[INST_SNARE]);
		outputs[GATE_OPEN_HIHAT_OUTPUT].setVoltage(amp[INST_OPEN_HIHAT]);
		outputs[GATE_CLOSED_HIHAT_OUTPUT].setVoltage(amp[INST_CLOSED_HIHAT]);

		outputs[VEL_KICK_OUTPUT].setVoltage(vel[INST_KICK]);
		outputs[VEL_SNARE_OUTPUT].setVoltage(vel[INST_SNARE]);
		outputs[VEL_OPEN_HIHAT_OUTPUT].setVoltage(vel[INST_OPEN_HIHAT]);
		outputs[VEL_CLOSED_HIHAT_OUTPUT].setVoltage(vel[INST_CLOSED_HIHAT]);
		

		prev_clock_input = clock_input;
		prev_reset_input = reset_input;
	}
};

struct GrooveBoxWidget : ModuleWidget {
	Label* labelPitch;

	GrooveBoxWidget(GrooveBox* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/groovebox.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));

		float spacingX = 30; //box.size.x / (float)4.0f;
		int x0 = spacingX;
		//int x1 = box.size.x - spacingX;
		int x1 = x0 + spacingX;
		int x2 = x1 + spacingX;
		int x3 = x2 + spacingX;

		float y = 72;
		addInput(createInputCentered<PJ301MPort>(Vec(x0, y), module, GrooveBox::CLOCK_INPUT));
		addInput(createInputCentered<PJ301MPort>(Vec(x1, y), module, GrooveBox::RESET_INPUT));
		addParam(createParamCentered<RoundBlackKnob>(Vec(x3, y), module, GrooveBox::ROOT_NOTE_PARAM));

		y = 142;
		addOutput(createOutputCentered<PJ301MPort>(Vec(x0, y), module, GrooveBox::GATE_BASS_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(x1, y), module, GrooveBox::BASS_OUTPUT));

		y = 170;
		addOutput(createOutputCentered<PJ301MPort>(Vec(x0, y), module, GrooveBox::GATE_MELODY_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(x1, y), module, GrooveBox::MELODY_OUTPUT));

		y = 190;
		addOutput(createOutputCentered<PJ301MPort>(Vec(x0, y), module, GrooveBox::GATE_OPEN_HIHAT_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(x1, y), module, GrooveBox::VEL_OPEN_HIHAT_OUTPUT));

		y = 238;
		addOutput(createOutputCentered<PJ301MPort>(Vec(x0, y), module, GrooveBox::GATE_CLOSED_HIHAT_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(x1, y), module, GrooveBox::VEL_CLOSED_HIHAT_OUTPUT));

		y = 296;
		addOutput(createOutputCentered<PJ301MPort>(Vec(x0, y), module, GrooveBox::GATE_SNARE_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(x1, y), module, GrooveBox::VEL_SNARE_OUTPUT));

		y = 344;
		addOutput(createOutputCentered<PJ301MPort>(Vec(x0, y), module, GrooveBox::GATE_KICK_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(x1, y), module, GrooveBox::VEL_KICK_OUTPUT));
		//addInput(createInputCentered<PJ301MPort>(Vec(x0, y), module, GrooveBox::CLOCK_INPUT));
		//addInput(createInputCentered<PJ301MPort>(Vec(x1, y), module, GrooveBox::RESET_INPUT));
		//addOutput(createOutputCentered<PJ301MPort>(Vec(x1, y), module, GrooveBox::RIGHT_OUTPUT));
		y = 72;
		for (int i = 0; i < 16; i++) {
			int x = x3 + i * spacingX;
			addParam(createParamCentered<BefacoTinyKnob>(Vec(x, y), module, i));

			int y2 = y + 30;
			addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<WhiteLight>>>((Vec(x, y2)), module, i + 16, GrooveBox::INVERT_LIGHT));
		}

		

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
		/*
		GrooveBox * module = dynamic_cast<GrooveBox*>(this->module);
		if (module) {
			labelPitch->text = module->getRootNoteName();
		}
		*/
		ModuleWidget::step();
	}
};

Model* modelGrooveBox = createModel<GrooveBox, GrooveBoxWidget>("groovebox");
