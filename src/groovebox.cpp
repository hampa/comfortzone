#include "plugin.hpp"
#include "grooves.h"
#include "oscillator.h"
#include <math.h>

#define LERP(a, b, f) (a * (1.0f - f)) + (b * f)
#define LOG0001 -9.210340371976182f // -80dB
#define CLAMP(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

struct GrooveBox : Module {
	enum ParamIds {
		KICK_PARAM = 0,
		SNARE_PARAM = 64,
		CLOSED_HIHAT_PARAM = 128,
		OPEN_HIHAT_PARAM = 192,
		ALL_PARAMS = 256,
		ROOT_NOTE_PARAM,
		SONG_A_PARAM,
		SONG_B_PARAM,
		SAVE_PARAM,
		NUM_PARAMS
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
		NUM_LIGHTS = 256 
	};

	const char *db = "/tmp/g.txt";

	GrooveBox () {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(ROOT_NOTE_PARAM, -12.0f, 12.f, 0, "Root Note", " Midi");
		configParam(SONG_A_PARAM, 0, NUM_TRACKS - 1, 0, "Song A", "");
		configParam(SONG_B_PARAM, 0, NUM_TRACKS - 1, 0, "Song B", "");
		configParam(SAVE_PARAM, 0, 1, 0, "Save", "");

		for (int i = 0; i < ALL_PARAMS; i++) {
			configParam(i, 0, 1.0f, 0.0f, "k", "", 0.f, 127.0f);
		}

		configInput(CLOCK_INPUT, "Clock Input");
		configInput(RESET_INPUT, "Reset Input");

		import_array(db);
	}

	~GrooveBox () {
		export_array(db);
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override {
		osc_melody.Init(e.sampleRate); osc_bass.Init(e.sampleRate);
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
	int track_a_idx = 0;
	int track_b_idx = 0;
	int section_idx = 0;
	Oscillator osc_bass;
	Oscillator osc_melody;

	inline float fmap_linear(float in, float min, float max) {
		return CLAMP(min + in * (max - min), min, max);
	}

	inline float fmap_exp(float in, float min, float max) {
		return CLAMP(min + (in * in) * (max - min), min, max);
	}

	void save_header(const char* headerName) {
		FILE *headerFile = fopen(headerName, "w");
		if (headerFile == NULL) {
			perror("Error opening file");
			return;
		}

		fprintf(headerFile, "float grooves[8][NUM_INST][64] = {\n");

		for (int i = 0; i < 8; i++) {
			fprintf(headerFile, "    {\n");
			for (int j = 0; j < NUM_INST; j++) {
				fprintf(headerFile, "        {");
				for (int k = 0; k < 64; k++) {
					float val = grooves[i][j][k];
					if (val == 0) {
						fprintf(headerFile, "0"); 
					}
					else {
						fprintf(headerFile, "%.0f", val); 
					}
					if (k < 63) fprintf(headerFile, ", ");
				}
				fprintf(headerFile, "}%s\n", j < NUM_INST - 1 ? "," : "");
			}
			fprintf(headerFile, "    }%s\n", i < 7 ? "," : "");
		}

		fprintf(headerFile, "};\n");
		fclose(headerFile);
	}

	void export_array(const char* filename) {
		FILE *file = fopen(filename, "w");
		if (file == NULL) {
			perror("Error opening file");
			return;
		}

		for (int i = 0; i < 8; i++) {
			for (int j = 0; j < NUM_INST; j++) {
				for (int k = 0; k < 64; k++) {
					fprintf(file, "%.0f ", grooves[i][j][k]);
				}
				fprintf(file, "\n");
			}
		}

		fclose(file);
	}

	void import_array(const char* filename) {
		FILE *file = fopen(filename, "r");
		if (file == NULL) {
			perror("Error opening file");
			return;
		}

		for (int i = 0; i < 8; i++) {
			for (int j = 0; j < NUM_INST; j++) {
				for (int k = 0; k < 64; k++) {
					if (fscanf(file, "%f", &grooves[i][j][k]) != 1) {
						fprintf(stderr, "Error reading value at [%d][%d][%d]\n", i, j, k);
						fclose(file);
						return;
					}
				}
			}
		}
		fclose(file);
	}

	inline float fmap_log(float in, float min, float max) {
		const float a = 1.f / log10f(max / min);
		return CLAMP(min * powf(10, in / a), min, max);
	}

	inline float mtof(float m) {
		return powf(2, (m - 69.0f) / 12.0f) * 440.0f;
	}
	bool force_load = true;
	bool force_save = false;

	void process(const ProcessArgs& args) override {
		float clock_input = inputs[CLOCK_INPUT].getVoltage(0);
		float reset_input = inputs[RESET_INPUT].getVoltage(0);
		float root_note = floorf((params[ROOT_NOTE_PARAM].getValue())) + 21;
		int song_a = floorf(params[SONG_A_PARAM].getValue());
		int track_b_idx = floorf(params[SONG_B_PARAM].getValue());
		if (track_a_idx != song_a) {
			track_a_idx = song_a;
			force_load = true;
		}

		int save = floorf(params[SAVE_PARAM].getValue());
		if (save && force_save == false) {
			force_save = true;
		}
		if (force_save) {
			save_header("/tmp/header.h");
			export_array(db);
			force_save = false;
		}

		if (force_load) {
			int p = 0;
			for (int i = 0; i < NUM_INST; i++) {
				for (int x = 0; x < 64; x++) {
					params[p].setValue(grooves[track_a_idx][i][x]);
					p++;
				}
			}
			force_load = false;
		}
		else {
			int p = 0;
			for (int i = 0; i < NUM_INST; i++) {
				for (int x = 0; x < 64; x++) {
					grooves[track_a_idx][i][x] = params[p].getValue();
					p++;
				}
			}
		}

		bool have_clock = false;
		if (prev_clock_input < 1 && clock_input > 1) {
			have_clock = true;
			current_step++;
			if (current_step >= 64) {
				current_step = 0;
				if (section_idx == 0) {
					section_idx = 1;
				}
				else {
					section_idx = 0;
				}
			}
		}
		if (prev_reset_input < 1 && reset_input > 1) {
			current_step = 0;
		}

		if (have_clock) {
			for (int i = 0; i < NUM_INST; i++) {
				float v = grooves[track_a_idx][i][current_step];
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
			if (grooves[track_a_idx][INST_OPEN_HIHAT][current_step]) {
				gateon[INST_CLOSED_HIHAT] = 0;
			}
			if (grooves[track_a_idx][INST_KICK][current_step]) {
				bass_gateon = 0;
			}
			else {
				bass_gateon = 30;
			}
			int bass_note, melody_note, melody_vel;
			if (section_idx == 0) {
				bass_note = bass_a[track_a_idx][current_step]; 
				melody_note = melody_a[track_a_idx][current_step];
				melody_vel = volume_a[track_a_idx][current_step];
			}
			else {
				bass_note = bass_b[track_b_idx][current_step]; 
				melody_note = melody_b[track_b_idx][current_step];
				melody_vel = volume_b[track_b_idx][current_step];
			}
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
			int p = 0;
			for (int i = 0; i < NUM_INST; i++) {
				for (int x = 0; x < 64; x++) {
					lights[p].setBrightness(params[p].getValue());
					p++;
				}
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
		int x4 = x3 + spacingX;

		float y = 72;
		addInput(createInputCentered<PJ301MPort>(Vec(x0, y), module, GrooveBox::CLOCK_INPUT));
		addInput(createInputCentered<PJ301MPort>(Vec(x1, y), module, GrooveBox::RESET_INPUT));
		addParam(createParamCentered<RoundBlackKnob>(Vec(x3, y), module, GrooveBox::ROOT_NOTE_PARAM));
		addParam(createParamCentered<VCVButton>(Vec(x4, y), module, GrooveBox::SAVE_PARAM));

		y = 142;
		addOutput(createOutputCentered<PJ301MPort>(Vec(x0, y), module, GrooveBox::GATE_BASS_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(x1, y), module, GrooveBox::BASS_OUTPUT));
		addParam(createParamCentered<RoundBlackKnob>(Vec(x3, y), module, GrooveBox::SONG_A_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(Vec(x4, y), module, GrooveBox::SONG_B_PARAM));

		y = 170;
		addOutput(createOutputCentered<PJ301MPort>(Vec(x0, y), module, GrooveBox::GATE_MELODY_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(x1, y), module, GrooveBox::MELODY_OUTPUT));

		y = 190;
		addOutput(createOutputCentered<PJ301MPort>(Vec(x0, y), module, GrooveBox::GATE_OPEN_HIHAT_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(x1, y), module, GrooveBox::VEL_OPEN_HIHAT_OUTPUT));
		addRow(module, x4, y, GrooveBox::OPEN_HIHAT_PARAM);

		y = 238;
		addOutput(createOutputCentered<PJ301MPort>(Vec(x0, y), module, GrooveBox::GATE_CLOSED_HIHAT_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(x1, y), module, GrooveBox::VEL_CLOSED_HIHAT_OUTPUT));
		addRow(module, x4, y, GrooveBox::CLOSED_HIHAT_PARAM);

		y = 296;
		addOutput(createOutputCentered<PJ301MPort>(Vec(x0, y), module, GrooveBox::GATE_SNARE_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(x1, y), module, GrooveBox::VEL_SNARE_OUTPUT));
		addRow(module, x4, y, GrooveBox::SNARE_PARAM);

		y = 344;
		addOutput(createOutputCentered<PJ301MPort>(Vec(x0, y), module, GrooveBox::GATE_KICK_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(x1, y), module, GrooveBox::VEL_KICK_OUTPUT));
		addRow(module, x4, y, GrooveBox::KICK_PARAM);

		//addInput(createInputCentered<PJ301MPort>(Vec(x0, y), module, GrooveBox::CLOCK_INPUT));
		//addInput(createInputCentered<PJ301MPort>(Vec(x1, y), module, GrooveBox::RESET_INPUT));
		//addOutput(createOutputCentered<PJ301MPort>(Vec(x1, y), module, GrooveBox::RIGHT_OUTPUT));
			/*
		y = 72;
		spacingX = 18;
		int param = 0;
		int y2 = y + 30;
		for (int z = 0; z < NUM_INST; z++) {
			for (int i = 0; i < 64; i++) {
				int x = x4 + i * spacingX;
				//addParam(createParamCentered<BefacoTinyKnob>(Vec(x, y), module, i));
				addParam(createLightParamCentered<VCVLightLatch<SmallSimpleLight<WhiteLight>>>((Vec(x, y2)), module, param, param));
				param++;
			}
			y2 += 30;
		}
			*/
		//addRow(module, x4, y2, 0);


		labelPitch = createWidget<Label>(Vec(5, 90));
		labelPitch->box.size = Vec(50, 50);
		labelPitch->text = "#F";
		labelPitch->color = metallic4;
		addChild(labelPitch);
	}

	void addLightRow(GrooveBox *module, int x, int y, int param) {

	}

	float steppos[64];
	void addRow(GrooveBox *module, int x, int y, int param) {
		for (int i = 0; i < 64; i++) {
			addParam(createLightParamCentered<VCVLightLatch<SmallSimpleLight<WhiteLight>>>((Vec(x, y)), module, param, param));
			steppos[i] = x;
			if (i == 0) {
				x += 18;
			}
			else if ((i % 16) == 15) {
				x += 30;
			}
			else if ((i % 4) == 3) { 
				x += 22;
			}
			else { 
				x += 18;
			}
			param++;
		}
	}

	// color scheme Echo Icon Theme Palette in Inkscape
	NVGcolor metallic2 = nvgRGBA(158, 171, 176, 255);
	NVGcolor metallic4 = nvgRGBA(14, 35, 46, 255);
	NVGcolor green1 = nvgRGBA(204, 255, 66, 255);
	NVGcolor red1 = nvgRGBA(255, 65, 65, 255);
	int current_step = 0;

	void step() override {
		GrooveBox * module = dynamic_cast<GrooveBox*>(this->module);
		if (module) {
			current_step = module->current_step;
		}

		ModuleWidget::step();
	}

	void draw(const DrawArgs &args) override {
		ModuleWidget::draw(args);
		if (module == NULL) {
			return;
		}
		float x = steppos[current_step];
		drawBox(args.vg, x, 80, 2, 2);
	}

	void drawBox(NVGcontext* vg, float x, float y, float width, float height) {
		nvgFillColor(vg, nvgRGBA(255, 192, 0, 255));
		nvgStrokeColor(vg, nvgRGBA(0, 0, 0, 255));
		nvgStrokeWidth(vg, 2.0f);

		nvgBeginPath(vg);
		nvgRect(vg, x, y, width, height);
		nvgFill(vg);
		nvgStroke(vg);
	}
};

Model* modelGrooveBox = createModel<GrooveBox, GrooveBoxWidget>("groovebox");
