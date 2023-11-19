#include "plugin.hpp"
#include "grooves.h"
#include "oscillator.h"
#include <math.h>

#define LERP(a, b, f) (a * (1.0f - f)) + (b * f)
#define LOG0001 -9.210340371976182f // -80dB
#define CLAMP(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))
enum {
	SECTION_INTRO,
	SECTION_VERSE,
	SECTION_CHORUS,
	SECTION_BRIDGE,
	SECTION_OUTRO,
	NUM_SECTIONS
};

#define NUM_8BARS 64
#define NUM_INST_PARAMS 384 

struct GrooveBox : Module {
	enum ParamIds {
		//KICK_PARAM = 0,
		//SNARE_PARAM = 64,
		//CLOSED_HIHAT_PARAM = 128,
		//OPEN_HIHAT_PARAM = 192,
		//CYMBAL_PARAM = 256,
		//PERC_PARAM = 320,

		//SECTION_PARAM = 64,
		//JUMP_PARAM = 128, 
		ROOT_NOTE_PARAM, 
		TRACK_PARAM,
		GROOVE_PARAM,
		MELODY_PARAM,
		SAVE_PARAM,
		LOOP_PARAM,
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
		GATE_CYMBAL_OUTPUT,
		GATE_EXTRA_OUTPUT,
		NUM_OUTPUTS
	};

	enum LightIds {
		//PARAM_LIGHT = 256,
		LOOP_LIGHT,
		NUM_LIGHTS 
	};

	GrooveBox () {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(ROOT_NOTE_PARAM, -12.0f, 12.f, 0, "Root Note", " Midi");
		configParam(TRACK_PARAM, 0, NUM_TRACKS - 1, 0, "Track", "");
		configParam(GROOVE_PARAM, 0, NUM_GROOVES - 1, 0, "Groove", "");
		configParam(SAVE_PARAM, 0, 1, 0, "Save", "");
		configParam(LOOP_PARAM, 0, 1, 0, "Loop", "");
		configParam(MELODY_PARAM, 0, NUM_MELODIES - 1, 0, "Melody", "");

		/*
		for (int i = 0; i < NUM_INST_PARAMS; i++) {
			configParam(i, 0, 1.0f, 0.0f, "k", "", 0.f, 127.0f);
		}
		*/

		/*
		for (int i = 0; i < NUM_8BARS; i++) {
			configParam(i + SECTION_PARAM, 0, NUM_SECTIONS - 1, 0, "Section", "");
		}

		for (int i = 0; i < NUM_8BARS; i++) {
			configParam(i + JUMP_PARAM, 0, 1, 0, "Jump", "");
		}
		*/

		configInput(CLOCK_INPUT, "Clock Input");
		configInput(RESET_INPUT, "Reset Input");
	}

	~GrooveBox () {
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override {
		osc_melody.Init(e.sampleRate); osc_bass.Init(e.sampleRate);
		osc_melody.SetWaveform(Oscillator::WAVE_SAW);
		osc_bass.SetWaveform(Oscillator::WAVE_SAW);
	}

	int current_step = 0;
	int current_section = 0;
	int current_groove = 0;
	int current_track = 0;
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
	int track_idx = 0;
	int groove_idx = 0;
	int section_idx = 0;
	int melody_idx = 0;
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

	inline float fmap_log(float in, float min, float max) {
		const float a = 1.f / log10f(max / min);
		return CLAMP(min * powf(10, in / a), min, max);
	}

	inline float mtof(float m) {
		return powf(2, (m - 69.0f) / 12.0f) * 440.0f;
	}
	bool force_load = true;
	bool force_save = false;

	void jumpToSection(int section) {
		current_section = section;
		current_step = 0;
	}

	void process(const ProcessArgs& args) override {
		float clock_input = inputs[CLOCK_INPUT].getVoltage(0);
		float reset_input = inputs[RESET_INPUT].getVoltage(0);
		float root_note = floorf((params[ROOT_NOTE_PARAM].getValue())) + 21;
		int track = floorf(params[TRACK_PARAM].getValue());
		int groove = floorf(params[GROOVE_PARAM].getValue());
		int is_looping = floorf(params[LOOP_PARAM].getValue()) > 0;
		int melody_idx = floorf(params[MELODY_PARAM].getValue());

		if (track_idx != track || groove_idx != groove) {
			track_idx = track;
			groove_idx = groove;
			current_groove = groove;
			current_track = track;
			force_load = true;
		}

		/*
		for (int i = 0; i < 64; i++) {
			if (params[JUMP_PARAM + i].getValue() > 0) {
				current_section = i;
				current_step = 0;
				break;
			}
		}
		*/

		int save = floorf(params[SAVE_PARAM].getValue());
		if (save && force_save == false) {
			force_save = true;
		}
		if (force_save) {
			save_header("/tmp/header.h");
			force_save = false;
		}

		/*
		if (force_load) {
			int p = 0;
			for (int i = 0; i < NUM_INST; i++) {
				for (int x = 0; x < 64; x++) {
					params[p].setValue(grooves[groove_idx][i][x]);
					p++;
				}
			}
			force_load = false;
		}
		else {
			int p = 0;
			for (int i = 0; i < NUM_INST; i++) {
				for (int x = 0; x < 64; x++) {
					grooves[groove_idx][i][x] = params[p].getValue();
					p++;
				}
			}
		}
		*/

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
				if (is_looping == false) {
					current_section++;
					if (current_section > 64) {
						current_section = 0;
					}
				}
			}
		}
		if (prev_reset_input < 1 && reset_input > 1) {
			current_step = 0;
		}

		if (have_clock) {
			for (int i = 0; i < NUM_INST; i++) {
				float v = grooves[groove_idx][i][current_step];
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
			if (grooves[groove_idx][INST_OPEN_HIHAT][current_step]) {
				gateon[INST_CLOSED_HIHAT] = 0;
			}
			if (grooves[groove_idx][INST_KICK][current_step]) {
				bass_gateon = 0;
			}
			else {
				bass_gateon = 30;
			}
			int bass_note, melody_note, melody_vel;
			bass_note = basses[melody_idx][current_step]; 
			melody_note = melodies[melody_idx][current_step];
			melody_vel = volumes[melody_idx][current_step];

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
			/*
			int p = 0;
			for (int i = 0; i < NUM_INST; i++) {
				for (int x = 0; x < 64; x++) {
					lights[p].setBrightness(params[p].getValue());
					p++;
				}
			}
			*/
			lights[LOOP_LIGHT].setBrightness(params[LOOP_PARAM].getValue());
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

struct JumpButton : Widget {
	GrooveBox *module = NULL;
	int squareSize = 10; 
	int rows = 1;
	int cols = 64;
	int mouse_row = -1;
	int mouse_col = -1;

	JumpButton() {
		box.size = Vec(rows * squareSize, rows * squareSize);
	}

	int getHeight() {
		return rows * squareSize;
	}	

	void onButton(const event::Button& e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			int row = e.pos.y / squareSize;
			int col = e.pos.x / squareSize;

			if (row >= 0 && row < rows && col >= 0 && col < cols) {
				module->jumpToSection(col);
				e.consume(this);
				return;
			}
		}
	}

	void onHover(const event::Hover& e) override {
		int row = e.pos.y / squareSize;
		int col = e.pos.x / squareSize;

		if (row >= 0 && row < rows && col >= 0 && col < cols) {
			mouse_row = row;
			mouse_col = col;
			e.consume(this); 
		}
		else {
			mouse_row = -1;
			mouse_col = -1;
		}
	}

	void onLeave(const event::Leave& e) override {
		mouse_row = -1;
		mouse_col = -1;
	}


	void draw(const DrawArgs &args) override {
		if (module == NULL) return;
		drawGrid(args.vg);
	}

	void drawGrid(NVGcontext* vg) {
		NVGcolor color;
		int i = 0;
		for (int j = 0; j < cols; ++j) {
			if (mouse_row == i && mouse_col == j) {
				color = nvgRGBA(255, 255, 255, 255); //grooves[module->current_groove][i][j] > 0 ? nvgRGBA(200, 0, 0, 255) : nvgRGBA(0, 0, 0, 200);
			}
			else {
				color = module->current_section == j ? nvgRGBA(0, 255, 255, 255) : nvgRGBA(0, 0, 0, 255);
			}

			float x = j * squareSize;
			float y = i * squareSize;

			nvgBeginPath(vg);
			nvgRect(vg, x, y, squareSize, squareSize);

			nvgFillColor(vg, color);
			nvgFill(vg);
		}
	}
};


struct SectionButton : Widget {
	GrooveBox *module = NULL;
	int squareSize = 10; 
	int rows = 6;
	int cols = 64;
	int mouse_row = -1;
	int mouse_col = -1;

	SectionButton() {
		box.size = Vec(cols * squareSize, rows * squareSize);
	}

	int getHeight() {
		return rows * squareSize;
	}

	void onButton(const event::Button& e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			int row = rows - e.pos.y / squareSize;
			int col = e.pos.x / squareSize;

			if (row >= 0 && row < rows && col >= 0 && col < cols) {
				tracks[module->current_track][0][col] = row; 
				e.consume(this);
				return;
			}
		}
	}

	void onHover(const event::Hover& e) override {
		int row = rows - e.pos.y / squareSize;
		int col = e.pos.x / squareSize;

		if (row >= 0 && row < rows && col >= 0 && col < cols) {
			mouse_row = row;
			mouse_col = col;
			e.consume(this); 
		}
		else {
			mouse_row = -1;
			mouse_col = -1;
		}
	}

	void onLeave(const event::Leave& e) override {
		mouse_row = -1;
		mouse_col = -1;
	}


	void draw(const DrawArgs &args) override {
		if (module == NULL) return;
		drawGrid(args.vg);
	}

	void drawGrid(NVGcontext* vg) {
		NVGcolor color;
		for (int i = 0; i < rows; ++i) {
			for (int j = 0; j < cols; ++j) {
				if (mouse_row == i && mouse_col == j) {
					color = nvgRGBA(255, 255, 255, 255); //grooves[module->current_groove][i][j] > 0 ? nvgRGBA(200, 0, 0, 255) : nvgRGBA(0, 0, 0, 200);
				}
				else {
					color = tracks[module->current_track][0][j] == i ? nvgRGBA(0, 255, 0, 255) : nvgRGBA(0, 0, 0, 255);
				}

				float x = j * squareSize;
				float y = (rows - i) * squareSize;

				nvgBeginPath(vg);
				nvgRect(vg, x, y, squareSize, squareSize);

				nvgFillColor(vg, color);
				nvgFill(vg);
			}
		}
	}
};

struct GridButton : Widget {
	GrooveBox *module = NULL;
	int squareSize = 10; 
	int rows = 6;
	int cols = 64;
	int mouse_row = -1;
	int mouse_col = -1;

	GridButton() {
		box.size = Vec(64 * squareSize, 6 * squareSize);
	}

	void onButton(const event::Button& e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			int row = e.pos.y / squareSize;
			int col = e.pos.x / squareSize;

			// Check if the click is within the grid bounds
			if (row >= 0 && row < rows && col >= 0 && col < cols) {
				if (grooves[module->current_groove][row][col] > 0) {
					grooves[module->current_groove][row][col] = 0;
				}
				else {
					grooves[module->current_groove][row][col] = 1;
				}
				e.consume(this); // Consume the event if it's within the grid
				return;
			}
		}
	}

	void onHover(const event::Hover& e) override {
		int row = e.pos.y / squareSize;
		int col = e.pos.x / squareSize;

		if (row >= 0 && row < rows  && col >= 0 && col < cols) {
			mouse_row = row;
			mouse_col = col;	
			e.consume(this); 
		}
		else {
			mouse_row = -1;
			mouse_col = -1;
		}
	}

	void onLeave(const event::Leave& e) override {
		mouse_row = -1;
		mouse_col = -1;
	}


	void draw(const DrawArgs &args) override {
		if (module == NULL) return;
		drawGrid(args.vg);
	}

	void drawGrid(NVGcontext* vg) {
		NVGcolor color;
		NVGcolor back;
		for (int i = 0; i < rows; ++i) {
			for (int j = 0; j < cols; ++j) {
				if (j == module->current_step) {
					back = nvgRGBA(100, 100, 100, 255);
				}
				else {
					back = nvgRGBA(0, 0, 0, 255);
				}

				if (mouse_row == i && mouse_col == j) {
					color = nvgRGBA(255, 255, 255, 255); 
				}
				else {
					color = grooves[module->current_groove][i][j] > 0 ? nvgRGBA(255, 0, 0, 255) : back;
				}

				float x = j * squareSize;
				float y = i * squareSize;

				nvgBeginPath(vg);
				nvgRect(vg, x, y, squareSize, squareSize);

				nvgFillColor(vg, color);
				nvgFill(vg);
			}
		}
	}
	/*
	   void draw(const DrawArgs &args) override {
	// Draw the grid or squares here
	// ...
	}
	*/
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
		int x5 = x4 + spacingX;
		int x6 = x5 + spacingX;

		float y = 72;
		addInput(createInputCentered<PJ301MPort>(Vec(x0, y), module, GrooveBox::CLOCK_INPUT));
		addInput(createInputCentered<PJ301MPort>(Vec(x1, y), module, GrooveBox::RESET_INPUT));
		addParam(createParamCentered<RoundBlackKnob>(Vec(x3, y), module, GrooveBox::ROOT_NOTE_PARAM));
		addParam(createParamCentered<VCVButton>(Vec(x4, y), module, GrooveBox::SAVE_PARAM));

		addParam(createLightParamCentered<VCVLightLatch<SmallSimpleLight<WhiteLight>>>((Vec(x5, y)), module, GrooveBox::LOOP_PARAM, GrooveBox::LOOP_LIGHT));

		y = 142;
		addOutput(createOutputCentered<PJ301MPort>(Vec(x0, y), module, GrooveBox::GATE_BASS_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(x1, y), module, GrooveBox::BASS_OUTPUT));
		addParam(createParamCentered<RoundBlackKnob>(Vec(x3, y), module, GrooveBox::TRACK_PARAM));

		//addSectionRow(module, x4, y, GrooveBox::SECTION_PARAM);

		y = 170;
		addOutput(createOutputCentered<PJ301MPort>(Vec(x0, y), module, GrooveBox::GATE_MELODY_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(x1, y), module, GrooveBox::MELODY_OUTPUT));
		addParam(createParamCentered<RoundBlackKnob>(Vec(x3, y), module, GrooveBox::GROOVE_PARAM));
		//addJumpRow(module, x4, y, GrooveBox::JUMP_PARAM);

		y = 190;
		addOutput(createOutputCentered<PJ301MPort>(Vec(x0, y), module, GrooveBox::GATE_OPEN_HIHAT_OUTPUT));
		//addOutput(createOutputCentered<PJ301MPort>(Vec(x1, y), module, GrooveBox::VEL_OPEN_HIHAT_OUTPUT));
		//addRow(module, x4, y, GrooveBox::OPEN_HIHAT_PARAM);

		y = 238 + 12;
		addOutput(createOutputCentered<PJ301MPort>(Vec(x0, y), module, GrooveBox::GATE_CLOSED_HIHAT_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(x1, y), module, GrooveBox::VEL_CLOSED_HIHAT_OUTPUT));
		//addRow(module, x4, y, GrooveBox::CLOSED_HIHAT_PARAM);

		y = 296 + 12;
		//addOutput(createOutputCentered<PJ301MPort>(Vec(x1, y), module, GrooveBox::VEL_SNARE_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(x0, y), module, GrooveBox::GATE_SNARE_OUTPUT));
		//addRow(module, x4, y, GrooveBox::SNARE_PARAM);

		y = 344;
		addOutput(createOutputCentered<PJ301MPort>(Vec(x0, y), module, GrooveBox::GATE_KICK_OUTPUT));
		//addRow(module, x4, y, GrooveBox::KICK_PARAM);

		y = 100;
        	SectionButton *sectionButton = createWidget<SectionButton>(Vec(150, y));
		sectionButton->module = module;
        	addChild(sectionButton);

		y += sectionButton->getHeight() + 30;

        	JumpButton *jumpButton = createWidget<JumpButton>(Vec(150, y));
		jumpButton->module = module;
        	addChild(jumpButton);

		y += jumpButton->getHeight() + 30;

        	GridButton *gridButton = createWidget<GridButton>(Vec(150, y));
		gridButton->module = module;
        	addChild(gridButton);

		//labelPitch = createWidget<Label>(Vec(5, 90));
		//labelPitch->box.size = Vec(50, 50);
		//labelPitch->text = "#F";
		//labelPitch->color = metallic4;
		//addChild(labelPitch);
	}

	void addLightRow(GrooveBox *module, int x, int y, int param) {

	}

	int getRowInc(int i) {
		if (i == 0) {
			return 18;
		}
		else if ((i % 16) == 15) {
			return 30;
		}
		else if ((i % 4) == 3) { 
			return 22;
		}
		return 18;
	}

	float steppos[64];
	void addRow(GrooveBox *module, int x, int y, int param) {
		for (int i = 0; i < 64; i++) {
			addParam(createLightParamCentered<VCVLightLatch<SmallSimpleLight<WhiteLight>>>((Vec(x, y)), module, param, param));
			steppos[i] = x;
			x += getRowInc(i);
			param++;
		}
	}

	void addSectionRow(GrooveBox *module, int x, int y, int param) {
		for (int i = 0; i < NUM_8BARS; i++) {
			addParam(createParamCentered<Trimpot>(Vec(x, y), module, param));
			x += getRowInc(i);
			param++;
		}
	}

	void addJumpRow(GrooveBox *module, int x, int y, int param) {
		for (int i = 0; i < NUM_8BARS; i++) {
			addParam(createParamCentered<VCVButton>(Vec(x, y), module, param));
			x += getRowInc(i);
			param++;
		}
	}


	// color scheme Echo Icon Theme Palette in Inkscape
	NVGcolor metallic2 = nvgRGBA(158, 171, 176, 255);
	NVGcolor metallic4 = nvgRGBA(14, 35, 46, 255);
	NVGcolor green1 = nvgRGBA(204, 255, 66, 255);
	NVGcolor red1 = nvgRGBA(255, 65, 65, 255);
	int current_step = 0;
	int current_section = 0;
	int current_groove = 0;
	int current_track = 0;

	void step() override {
		GrooveBox * module = dynamic_cast<GrooveBox*>(this->module);
		if (module) {
			current_step = module->current_step;
			current_section = module->current_section;
			current_groove = module->current_groove;
		}

		ModuleWidget::step();

	}

    	int squareSize = 10; // The size of each square in the grid

	/*
	void onButton(const event::Button& e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			Vec mousePos = e.pos;

			int row = mousePos.y / squareSize;
			int col = mousePos.x / squareSize;

			if (row >= 0 && row < 6 && col >= 0 && col < 64) {
				if (grooves[current_groove][row][col] > 0) {
					grooves[current_groove][row][col] = 0;
				}
				else {
					grooves[current_groove][row][col] = 1;
				}
				e.consume(this);
				return;
			}
		}
		ModuleWidget::onButton(e);
	}
	*/


	void draw(const DrawArgs &args) override {
		ModuleWidget::draw(args);
		if (module == NULL) {
			return;
		}
		float x = steppos[current_step];
		drawBox(args.vg, x, 100, 2, 2);

		x = steppos[current_section];
		drawBox(args.vg, x, 80, 2, 2);
		//drawGrid(args.vg);
	}

	void drawGrid(NVGcontext* vg) {
		int rows = 6;
		int cols = 64;
		int squareSize = 10; // size of each square in the grid

		for (int i = 0; i < rows; ++i) {
			for (int j = 0; j < cols; ++j) {
				NVGcolor color = grooves[current_groove][i][j] > 0 ? nvgRGBA(255, 0, 0, 255) : nvgRGBA(0, 0, 0, 255);

				float x = j * squareSize;
				float y = i * squareSize;

				nvgBeginPath(vg);
				nvgRect(vg, x, y, squareSize, squareSize);

				nvgFillColor(vg, color);
				nvgFill(vg);
			}
		}
	}


	void drawBox(NVGcontext* vg, float x, float y, float width, float height) {

		//nvgBeginFrame(vg, windowWidth, windowHeight, pixelRatio);

		nvgFillColor(vg, nvgRGBA(255, 192, 0, 255));
		nvgStrokeColor(vg, nvgRGBA(0, 0, 0, 255));
		nvgStrokeWidth(vg, 2.0f);

		nvgBeginPath(vg);
		nvgRect(vg, x, y, width, height);
		nvgFill(vg);
		nvgStroke(vg);
		//nvgEndFrame(vg);
	}
};

Model* modelGrooveBox = createModel<GrooveBox, GrooveBoxWidget>("groovebox");
