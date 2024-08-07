#include "plugin.hpp"
#define WITH_BASS 1
#include "grooves.h"
#include "oscillator.h"
#include <errno.h>
#include <math.h>
#include <sys/stat.h>

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

#define SQUARE_SIZE 12

struct GrooveBox : Module {
	enum ParamIds {
		ROOT_NOTE_PARAM, 
		TRACK_PARAM,
		SAVE_PARAM,
		LOOP_PARAM,
		LOAD_PARAM,
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
		GATE_SNARE1_OUTPUT,
		GATE_SNARE2_OUTPUT,
		GATE_HIHAT_CLOSED1_OUTPUT,
		GATE_HIHAT_CLOSED2_OUTPUT,
		GATE_HIHAT_OPEN_OUTPUT,
		GATE_RIDE_OUTPUT,
		GATE_CRASH_OUTPUT,
		GATE_CLAP_OUTPUT,
		GATE_PERC_OUTPUT,
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
		configParam(SAVE_PARAM, 0, 1, 0, "Save", "");
		configParam(LOAD_PARAM, 0, 1, 0, "Load", "");
		configParam(LOOP_PARAM, 0, 1, 0, "Loop", "");
		configInput(CLOCK_INPUT, "Clock Input");
		configInput(RESET_INPUT, "Reset Input");

		configOutput(GATE_MELODY_OUTPUT, "Melody");
		configOutput(MELODY_OUTPUT, "Melody Voice");
		configOutput(GATE_BASS_OUTPUT, "Bass");
		configOutput(BASS_OUTPUT, "Bass Voice");
		configOutput(GATE_KICK_OUTPUT, "Kick");
		configOutput(GATE_SNARE1_OUTPUT, "Snare1");
		configOutput(GATE_SNARE2_OUTPUT, "Snare2");
		configOutput(GATE_HIHAT_CLOSED1_OUTPUT, "Hihat Closed1");
		configOutput(GATE_HIHAT_CLOSED2_OUTPUT, "Hihat Closed2");
		configOutput(GATE_HIHAT_OPEN_OUTPUT, "Hihat Open");
		configOutput(GATE_RIDE_OUTPUT, "Ride");
		configOutput(GATE_CRASH_OUTPUT, "Crash");
		configOutput(GATE_CLAP_OUTPUT, "Clap");
		configOutput(GATE_PERC_OUTPUT, "Perc");
		//import_array(get_path("db.txt"));
		//gen_groove_beat();
	}

	~GrooveBox () {
		//save_header(get_path("gb-header.h"));
		//export_array(get_path("db.txt"));
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override {
		osc_melody.Init(e.sampleRate); osc_bass.Init(e.sampleRate);
		osc_melody.SetWaveform(Oscillator::WAVE_SAW);
		osc_bass.SetWaveform(Oscillator::WAVE_SAW);
	}

	int current_step = 0;
	int current_track = 0;
	float prev_clock_input = -2;
	float prev_reset_input = -2;
	int gateon[NUM_INST];
	float amp[NUM_INST];
	float vel[NUM_INST];
	float melody_gateon = 0;
	float melody_amp = 0;
	float bass_freq = 110;
	float melody_freq = 220;
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

	const char *get_path(const char  *filename) {
		static char path[1024]; 
		//const char *doc = asset::user(pluginInstance->slug).c_str();
		//mkdir(doc, 0777);
#ifdef _WIN32
		mkdir(asset::user(pluginInstance->slug).c_str());
#else
		mkdir(asset::user(pluginInstance->slug).c_str(), 0777);
#endif
		memset(path, 0, sizeof(path));
		snprintf(path, sizeof(path), "%s/%s", asset::user(pluginInstance->slug).c_str(), filename);
		return path;
	}

	/*
	const char* get_path_old(const char *filename) {
		static char path[1024]; // Large enough buffer for the path

		const char* documentsPath = rack::asset::user("").c_str(); // Get the user documents directory
		const char* rackFolder = "/Rack"; // Subdirectory for Rack
		const char* pluginFolder = "/plugins/Comfortzone/"; // Your plugin's specific folder

		// Ensure the buffer is clear
		memset(path, 0, sizeof(path));

		strncpy(path, documentsPath, sizeof(path) - 1);
		strncat(path, rackFolder, sizeof(path) - strlen(path) - 1);
		strncat(path, pluginFolder, sizeof(path) - strlen(path) - 1);
		strncat(path, filename, sizeof(path) - strlen(path) - 1);

		return path;
	}
	*/

	void save_header(const char* headerName) {
		FILE *headerFile = fopen(headerName, "w");
		DEBUG("save header to the file %s", headerName);
		if (headerFile == NULL) {
			DEBUG("Error opening file %s", strerror(errno));
			return;
		}
		/*
		fprintf(headerFile, "#define NUM_TRACKS %i\n", NUM_TRACKS);
		fprintf(headerFile, "#define NUM_TRACK_SETTINGS %i\n", NUM_TRACK_SETTINGS);
		fprintf(headerFile, "#define NUM_GROOVES %i\n", NUM_GROOVES);
		fprintf(headerFile, "#define NUM_INST %i\n\n", NUM_INST);
		*/

		fprintf(headerFile, "char tracks[NUM_TRACKS][NUM_TRACK_SETTINGS][64] = {\n");
		for (int i = 0; i < NUM_TRACKS; i++) {
			fprintf(headerFile, "//track: %i\n", i);
			fprintf(headerFile, "    {\n");
			for (int j = 0; j < NUM_TRACK_SETTINGS; j++) {
				fprintf(headerFile, "        {");
				for (int k = 0; k < 64; k++) {
					//int val = tracks[i][j][k];
					fprintf(headerFile, "%i", tracks[i][j][k]); 
					if (k < 63) fprintf(headerFile, ",");
				}
				fprintf(headerFile, "}%s\n", j < NUM_INST - 1 ? "," : "");
			}
			fprintf(headerFile, "    }%s\n", i < 7 ? "," : "");
		}
		fprintf(headerFile, "};\n\n");

		fprintf(headerFile, "char grooves[NUM_GROOVES][NUM_INST][64] = {\n");

		for (int i = 0; i < NUM_GROOVES; i++) {
			fprintf(headerFile, "//groove: %i\n", i);
			fprintf(headerFile, "{\n");
			for (int j = 0; j < NUM_INST; j++) {
				fprintf(headerFile, " {");
				for (int k = 0; k < 64; k++) {
					fprintf(headerFile, "%i", grooves[i][j][k]); 
					if (k < 63) fprintf(headerFile, ",");
				}
				fprintf(headerFile, "}%s", j < NUM_INST - 1 ? "," : "");
				fprintf(headerFile, "//%s\n", inst[j]);
			}
			fprintf(headerFile, "}%s\n", i < NUM_GROOVES - 1 ? "," : "");
		}

		fprintf(headerFile, "};\n");
		fclose(headerFile);
	}

	void export_array(const char* filename) {
		FILE *file = fopen(filename, "w");
		if (file == NULL) {
			DEBUG("Error opening file %s", strerror(errno));
			return;
		}

		for (int i = 0; i < NUM_TRACKS; i++) {
			for (int j = 0; j < NUM_TRACK_SETTINGS; j++) {
				for (int k = 0; k < 64; k++) {
					fprintf(file, "%i ", tracks[i][j][k]);
				}
				fprintf(file, "\n");
			}
		}

		for (int i = 0; i < NUM_GROOVES; i++) {
			for (int j = 0; j < NUM_INST; j++) {
				for (int k = 0; k < 64; k++) {
					fprintf(file, "%i ", grooves[i][j][k]);
				}
				fprintf(file, "\n");
			}
		}
		fclose(file);
	}

	void import_array(const char* filename) {
		FILE *file = fopen(filename, "r");
		if (file == NULL) {
			DEBUG("Error opening file %s", strerror(errno));
			return;
		}
		DEBUG("import array %s", filename);

		for (int i = 0; i < NUM_TRACKS; i++) {
			for (int j = 0; j < NUM_TRACK_SETTINGS; j++) {
				for (int k = 0; k < 64; k++) {
					if (fscanf(file, "%i", &tracks[i][j][k]) != 1) {
						DEBUG("Error reading value at [%i][%i][%i]\n", i, j, k);
					}
				}
			}
		}

		for (int i = 0; i < NUM_GROOVES; i++) {
			for (int j = 0; j < NUM_INST; j++) {
				for (int k = 0; k < 64; k++) {
					if (fscanf(file, "%i", &grooves[i][j][k]) != 1) {
						DEBUG("Error reading value at [%i][%i][%i]\n", i, j, k);
						fclose(file);
						return;
					}
					else {
						DEBUG("value at [%i][%i][%i] = %i\n", 
							i, j, k,
							grooves[i][j][k]);

					}
				}
			}
		}
		fclose(file);
	}
	enum {
		GRV_SILENT,
		GRV_KBBB,
		GRV_HIHAT,
		GRV_BASS
	};

	void gen_groove_beat() {
		int bar[64] = {
		0,0,0,0,

		1,1,1,1, // just kbbb, LEVEL 0
		2,2,2,2, // hihat, LEVEL 1
		2,2,2,2, // hihat
		2,2,2,2, // hihat 

		3,3,3,3, // bass with notes, LEVEL 2
		3,3,3,3, // bass with notes
		3,3,3,3, // bass with notes
		1,1,1,1, // just kbb 

		3,3,3,3, // bass with notes
		3,3,3,3, // bass with notes
		3,3,3,3, // bass with
		1,1,1,1, // just kicks

		3,3,3,3, // bass with notes
		1,1,1,1, // just kbb , LEVEL 2
		0,0,0,0
		};

		/*	
        	for (int i = 1; i < NUM_GROOVES; i += 4) {
               		grooves[i][INST_KICK] = 1;
		}
		*/
		
		for (int i = 0; i < NUM_TRACKS; i++) {
			//for (int j = 0; j < NUM_INST; j++) {
			for (int k = 0; k < 64; k++) {
				int g = bar[k];	
				if (i > 4 && g != 0) {
					g += 4;
				} 
				tracks[i][TRACK_SETTING_GROOVE][k] = g; 
				int x = k % 4;
				if (x == 3 && k > 4 && k < 63) {
					tracks[i][TRACK_SETTING_GROOVE][k] += 8;
				}
				//switch (bar[k]) {
				/*
				case GRV_SILENT:
					grooves[i][j][k] = 0;
					break;
				case GRV_KBBB:
					if (x == 0) {
					break;
				*/
				/*
				int on = 0;
				int j = 0;
				if (x == 0) {
					j = INST_SNARE1;
					on = 1;
				}
				else if (x == 3) {
					j = INST_HIHAT_CLOSED1;
					on = 1;
				}
				else if (x == 2) {
					j = INST_HIHAT_OPEN;
					on = 1;
				}
				grooves[i][j][k] = on;
				*/
			}
			//}
		}
	}

	inline float fmap_log(float in, float min, float max) {
		const float a = 1.f / log10f(max / min);
		return CLAMP(min * powf(10, in / a), min, max);
	}

	void gen_groove_melody() {
		int groove = 0;
		int melody = 0;
		for (int i = 0; i < NUM_TRACKS; i++) {
			for (int k = 0; k < 64; k++) {
				tracks[i][0][k] = groove;
				tracks[i][1][k] = melody;
			}
			melody++;
			groove++;
		}

		for (int i = 0; i < NUM_TRACKS; i++) {
			for (int k = 0; k < 64; k++) {
				/*
				   if ((k % 4) == 0) {
				   grooves[i][INST_PERC][k] = 1;
				   }
				 */
				if (k > 60) {
					grooves[i][INST_SNARE2][k] = 1;
				}
				if ((k % 8) == 4) {
					grooves[i][INST_SNARE1][k] = 1;
				}
				// offbeat
				if ((k % 4) == 2) {
					grooves[i][INST_HIHAT_OPEN][k] = 1;
				}
				else {
					grooves[i][INST_HIHAT_CLOSED1][k] = 1;
				}
				if (k > 60) {
					grooves[i][INST_KICK][k] = 1;
				}
				else if ((k % 4) == 0) {
					grooves[i][INST_KICK][k] = 1;
				}
				else {
					grooves[i][INST_BASS][k] = 1;
				} 
			}
		}
	}

	inline float mtof(float m) {
		return powf(2, (m - 69.0f) / 12.0f) * 440.0f;
	}

	void jumpToSection(int section) {
		section_idx = section;
		current_step = 0;
	}

	void process(const ProcessArgs& args) override {
		float clock_input = inputs[CLOCK_INPUT].getVoltage(0);
		float reset_input = inputs[RESET_INPUT].getVoltage(0);
		float root_note = floorf((params[ROOT_NOTE_PARAM].getValue())) + 21;
		int track = floorf(params[TRACK_PARAM].getValue());
		//int groove = floorf(params[GROOVE_PARAM].getValue());
		int is_looping = floorf(params[LOOP_PARAM].getValue()) > 0;
		//int melody_idx = floorf(params[MELODY_PARAM].getValue());

		if (current_track != track) {
			current_track = track;
		}

		bool save = params[SAVE_PARAM].getValue() > 0;
		if (save) {
			//save_header(get_path("gb-header.h"));
			//save_header("/tmp/header.h");
			save_header(get_path("groove2.h"));
			export_array(get_path("db.txt"));
			params[SAVE_PARAM].setValue(0);
		}

		bool load = params[LOAD_PARAM].getValue() > 0;
		if (load) {
			params[LOAD_PARAM].setValue(0);
			import_array(get_path("db.txt"));
			gen_groove_beat();
		}

		bool have_clock = false;
		if (prev_clock_input < 1 && clock_input > 1) {
			have_clock = true;
			current_step++;
			if (current_step >= 64) {
				current_step = 0;
				if (is_looping == false) {
					section_idx++;
					if (section_idx >= 64) {
						section_idx = 0;
					}
				}
			}
			groove_idx = tracks[current_track][0][section_idx];
			melody_idx = tracks[current_track][1][section_idx];
		}
		if (prev_reset_input < 1 && reset_input > 1) {
			current_step = 0;
		}

		if (have_clock) {
			for (int i = 0; i < NUM_INST; i++) {
				float v = grooves[groove_idx][i][current_step];
				if (v > 0) {
					gateon[i] = 30;
					/*
					   if (v > 1) {
					   v /= 127.0f;
					   }	
					   */
					vel[i] = 5.0f;
				}
				else {
					gateon[i] = 0;
				}
			}
			/*
			int oh = grooves[groove_idx][INST_OPEN_HIHAT][current_step];
			int h1 = grooves[groove_idx][INST_CLOSED_HIHAT][current_step];
			int h2 = grooves[groove_idx][INST_CLOSED_HIHAT2][current_step];
			if (oh > 0) {
				vel[INST_CLOSED_HIHAT] = 0;	
				gateon[INST_CLOSED_HIHAT] = 30;
			}
			else if (h1 > 0 && h2 > 0) {
				vel[INST_CLOSED_HIHAT] = 5;
				gateon[INST_CLOSED_HIHAT] = 30;
			}
			else if (h2 > 0) {
				vel[INST_CLOSED_HIHAT] = 3;
				gateon[INST_CLOSED_HIHAT] = 30; 
			}
			else if (h1 > 0) {
				vel[INST_CLOSED_HIHAT] = 1;
				gateon[INST_CLOSED_HIHAT] = 30;
			}
			*/

			if (grooves[groove_idx][INST_KICK][current_step]) {
				gateon[INST_BASS] = 0;
				vel[INST_BASS] = 0;
				osc_bass.Reset();
			}
			else if (grooves[groove_idx][INST_BASS][current_step]) {
				gateon[INST_BASS] = 30;
				vel[INST_BASS] = 1;
			}
			else {
				gateon[INST_BASS] = 0;
				vel[INST_BASS] = 0;
				osc_bass.Reset();
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

		if (melody_gateon > 1) {
			melody_gateon--;
			melody_amp = 5;
		}
		else {
			melody_gateon = 0;
			melody_amp = 0;
		}

		float melody = osc_melody.Process() * 5.0f;
		float bass = osc_bass.Process() * 5.0f * vel[INST_BASS];

		outputs[BASS_OUTPUT].setVoltage(bass);

		outputs[GATE_MELODY_OUTPUT].setVoltage(melody_amp);
		outputs[MELODY_OUTPUT].setVoltage(melody);

		outputs[GATE_KICK_OUTPUT].setVoltage(amp[INST_KICK]);
		outputs[GATE_SNARE1_OUTPUT].setVoltage(amp[INST_SNARE1]);
		outputs[GATE_SNARE2_OUTPUT].setVoltage(amp[INST_SNARE2]);

		outputs[GATE_HIHAT_CLOSED1_OUTPUT].setVoltage(amp[INST_HIHAT_CLOSED1]);
		outputs[GATE_HIHAT_CLOSED2_OUTPUT].setVoltage(amp[INST_HIHAT_CLOSED2]);
		outputs[GATE_HIHAT_OPEN_OUTPUT].setVoltage(amp[INST_HIHAT_OPEN]);

		outputs[GATE_RIDE_OUTPUT].setVoltage(amp[INST_RIDE]);
		outputs[GATE_CRASH_OUTPUT].setVoltage(amp[INST_CRASH]);
		outputs[GATE_CLAP_OUTPUT].setVoltage(amp[INST_CLAP]);
		outputs[GATE_PERC_OUTPUT].setVoltage(amp[INST_PERC]);
		
		prev_clock_input = clock_input;
		prev_reset_input = reset_input;
	}
};

struct JumpButton : Widget {
	GrooveBox *module = NULL;
	int squareSize = SQUARE_SIZE; 
	int cols = 64;
	int mouse_col = -1;

	JumpButton() {
		box.size = Vec(cols * squareSize, squareSize);
	}

	int getHeight() {
		return squareSize;
	}	

	void onButton(const event::Button& e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			int col = e.pos.x / squareSize;
			if (col >= 0 && col < cols) {
				module->jumpToSection(col);
				e.consume(this);
				return;
			}
		}
	}

	void onHover(const event::Hover& e) override {
		int col = e.pos.x / squareSize;

		if (col >= 0 && col < cols) {
			mouse_col = col;
			e.consume(this); 
		}
		else {
			mouse_col = -1;
		}
	}

	void onLeave(const event::Leave& e) override {
		mouse_col = -1;
	}

	void draw(const DrawArgs &args) override {
		if (module == NULL) return;
		drawGrid(args.vg);
	}

	void drawGrid(NVGcontext* vg) {
		NVGcolor color;
		for (int j = 0; j < cols; ++j) {
			if (mouse_col == j) {
				color = nvgRGBA(255, 255, 255, 255);
			}
			else {
				color = module->section_idx == j ? nvgRGBA(0, 255, 255, 255) : nvgRGBA(0, 0, 0, 255);
			}

			float x = j * squareSize;
			float y = 0; //i * squareSize;

			nvgBeginPath(vg);
			nvgRect(vg, x, y, squareSize, squareSize);

			nvgFillColor(vg, color);
			nvgFill(vg);
		}
	}
};


struct BinButton : Widget {
	GrooveBox *module = NULL;
	int squareSize = SQUARE_SIZE; 
	int rows = 4;
	int cols = 1;
	int mouse_row = -1;
	int mouse_col = -1;

	BinButton() {
		box.size = Vec(cols * squareSize, rows * squareSize);
	}

	int getHeight() {
		return rows * squareSize;
	}

	int toggleNthBit(int num, int N) {
    		return num ^ (1 << N);
	}


	bool isNthBitSet(int num, int N) {
    		return (num & (1 << N)) != 0;
	}

	void onButton(const event::Button& e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			 int row = e.pos.y / squareSize;
			 int col = e.pos.x / squareSize;

			if (row >= 0 && row < rows && col >= 0 && col < cols) {
				int c = module->groove_idx; 
				c = toggleNthBit(c, row);
				assert(c >= 0 && c < NUM_GROOVES);
				module->groove_idx = c;	
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
		
		for (int i = 0; i < rows; ++i) {
			for (int j = 0; j < cols; ++j) {
				if (mouse_row == i && mouse_col == j) {
					color = nvgRGBA(255, 255, 255, 255); 
				}
				else {
					color = isNthBitSet(module->groove_idx, i) ? nvgRGBA(0, 255, 0, 255) : nvgRGBA(0, 0, 0, 255);
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
};

struct SectionButton : Widget {
	GrooveBox *module = NULL;
	int config_idx = 0;
	int squareSize = SQUARE_SIZE; 
	int rows = 4;
	int cols = 64;
	int mouse_row = -1;
	int mouse_col = -1;

	SectionButton() {
		box.size = Vec(cols * squareSize, rows * squareSize);
	}

	int getHeight() {
		return rows * squareSize;
	}

	int toggleNthBit( int num, int N) {
    		return num ^ (1 << N);
	}

	bool isNthBitSet( int num, int N) {
    		return (num & (1 << N)) != 0;
	}

	int setNthBit(int num, int N) {
		return num | (1 << N);
	}

	void onButton(const event::Button& e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			 int row = e.pos.y / squareSize;
			 int col = e.pos.x / squareSize;

			if (row >= 0 && row < rows && col >= 0 && col < cols) {
				int c = tracks[module->current_track][config_idx][col];
				c = toggleNthBit(c, row);
				tracks[module->current_track][config_idx][col] = c;

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
			int modkeys = APP->window->getMods();
			if (modkeys & GLFW_MOD_SHIFT) {
				int c = tracks[module->current_track][config_idx][col];
				c = setNthBit(c, row);
				tracks[module->current_track][config_idx][col] = c;
			}
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

	bool show_doubles = true;

	void drawGrid(NVGcontext* vg) {
		NVGcolor color;
		NVGcolor back;

		int melodies[NUM_MELODIES];
		if (show_doubles) {
			for (int i = 0; i < NUM_MELODIES; i++) {
				melodies[i] = 0;
			}
			for (int i = 0; i < module->current_track; i++) {
				//if (i == module->current_track) continue;
				for (int j = 0; j < cols; ++j) {
					int m = tracks[i][config_idx][j];
					melodies[m]++;
				}
			}
		}

		for (int j = 0; j < cols; ++j) {
			int m = tracks[module->current_track][config_idx][j];
			for (int i = 0; i < rows; ++i) {
				if (j == module->current_step) {
					back = nvgRGBA(100, 100, 100, 255);
				}
				else if (j == 32) {
					back = nvgRGBA(50, 50, 50, 255);
				}
				else if ((j / 4) % 2) {
					back = nvgRGBA(30, 30, 30, 255);
				}
				else {
					back = nvgRGBA(0, 0, 0, 255);
				}

				if (mouse_row == i && mouse_col == j) {
					color = nvgRGBA(255, 255, 255, 255); 
				}
				else {
					//color = isNthBitSet(tracks[module->current_track][config_idx][j], i) ? 
					if (isNthBitSet(m, i)) {
						if (melodies[m] == 0) {
							color = nvgRGBA(0, 255, 0, 255);
						}
						else {
							color = nvgRGBA(255, 200, 0, 255);
						}
					}
					else {
						color = back;
					}
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
};

struct GridButton : Widget {
	GrooveBox *module = NULL;
	int squareSize = SQUARE_SIZE; 
	int rows = NUM_INST;
	int cols = 64;
	int mouse_row = -1;
	int mouse_col = -1;

	GridButton() {
		box.size = Vec(cols * squareSize, rows * squareSize);
	}

	void setAll(int row, int col, int val) {
		for (int i = 0; i < 64; i++) {
			if ((i % 4) == (col % 4)) {
				grooves[module->groove_idx][row][i] = val;
			}
		}
	}

	void onButton(const event::Button& e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			int row = e.pos.y / squareSize;
			int col = e.pos.x / squareSize;
			bool all = (glfwGetKey(APP->window->win, GLFW_KEY_A) == GLFW_PRESS);

			if (row >= 0 && row < rows && col >= 0 && col < cols) {
				assert (module->groove_idx < NUM_GROOVES && module->groove_idx >= 0);
				if (grooves[module->groove_idx][row][col] > 0) {
					grooves[module->groove_idx][row][col] = 0;
					if (all) {
						setAll(row, col, 0);
					}
				}
				else {
					grooves[module->groove_idx][row][col] = 1;
					if (all) {
						setAll(row, col, 1);
					}
				}
				e.consume(this);
				return;
			}
		}
	}

	int copy_groove_idx = 0;

	void onHover(const event::Hover& e) override {
		int row = e.pos.y / squareSize;
		int col = e.pos.x / squareSize;

		if (row >= 0 && row < rows && col >= 0 && col < cols) {
			mouse_row = row;
			mouse_col = col;	
			int modkeys = APP->window->getMods();
			if (modkeys & GLFW_MOD_SHIFT) {
				grooves[module->groove_idx][row][col] = 0;
        		}
			if  (modkeys & GLFW_MOD_CONTROL || modkeys & GLFW_MOD_SUPER) {
				//DEBUG("mod control");
				if (glfwGetKey(APP->window->win, GLFW_KEY_C) == GLFW_PRESS) {
					copy_groove_idx = module->groove_idx;
					//DEBUG("mod control copy");
				}
				else if (glfwGetKey(APP->window->win, GLFW_KEY_V) == GLFW_PRESS) {
					//DEBUG("mod control paste");
					for (int i = 0; i < rows; i++) {
						for (int j = 0; j < cols; j++) {
							grooves[module->groove_idx][i][j] = 
							grooves[copy_groove_idx][i][j];
						}
					}
				}
			}
			if (glfwGetKey(APP->window->win, GLFW_KEY_2) == GLFW_PRESS) {
				if ((col % 4) == 1) {
					grooves[module->groove_idx][row][col] = 1;
				}
        		}
			if (glfwGetKey(APP->window->win, GLFW_KEY_1) == GLFW_PRESS) {
				if ((col % 4) == 0) {
					grooves[module->groove_idx][row][col] = 1;
				}
			}
			if (glfwGetKey(APP->window->win, GLFW_KEY_3) == GLFW_PRESS) {
				if ((col % 4) == 2) {
					grooves[module->groove_idx][row][col] = 1;
				}
			}
			if (glfwGetKey(APP->window->win, GLFW_KEY_4) == GLFW_PRESS) {
				if ((col % 4) == 3) {
					grooves[module->groove_idx][row][col] = 1;
				}
			}
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

	NVGcolor metallic2 = nvgRGBA(158, 171, 176, 255);
	NVGcolor metallic4 = nvgRGBA(14, 35, 46, 255);
	NVGcolor green1 = nvgRGBA(204, 255, 66, 255);
	NVGcolor red1 = nvgRGBA(255, 65, 65, 255);

	NVGcolor colors[NUM_INST] = {
		red1,
		red1,
		green1,
		green1,
		green1,
		metallic2,
		metallic2,
		red1
	};

	void drawGrid(NVGcontext* vg) {
		NVGcolor color;
		NVGcolor back;
		for (int i = 0; i < rows; ++i) {
			for (int j = 0; j < cols; ++j) {
				if (j == module->current_step) {
					back = nvgRGBA(100, 100, 100, 255);
				}
				else if (j == 32) {
					back = nvgRGBA(50, 50, 50, 255);
				}
				else if ((j / 4) % 2) {
					back = nvgRGBA(30, 30, 30, 255);
				}
				else {
					back = nvgRGBA(0, 0, 0, 255);
				}

				if (mouse_row == i && mouse_col == j) {
					color = nvgRGBA(255, 255, 255, 255); 
				}
				else {
					color = grooves[module->groove_idx][i][j] > 0 ? 
						colors[i]: back;
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
};


struct GrooveBoxWidget : ModuleWidget {
	Label* labelInfo;

	GrooveBoxWidget(GrooveBox* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/groovebox.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));

		float spacingX = 25; //box.size.x / (float)4.0f;
		int x0 = 20;
		int x1 = x0 + spacingX;
		int x2 = x1 + spacingX;
		int x3 = x2 + spacingX;
		int x4 = x3 + spacingX;
		int x5 = x4 + spacingX;

		float y = 54;
		addInput(createInputCentered<PJ301MPort>(Vec(x0, y), module, GrooveBox::CLOCK_INPUT));
		addInput(createInputCentered<PJ301MPort>(Vec(x1, y), module, GrooveBox::RESET_INPUT));
		addParam(createParamCentered<RoundBlackKnob>(Vec(x3, y), module, GrooveBox::ROOT_NOTE_PARAM));
		addParam(createParamCentered<VCVButton>(Vec(x4, y), module, GrooveBox::SAVE_PARAM));

		addParam(createLightParamCentered<VCVLightLatch<SmallSimpleLight<WhiteLight>>>((Vec(x5, y)), module, GrooveBox::LOOP_PARAM, GrooveBox::LOOP_LIGHT));
		y = 90;
		addOutput(createOutputCentered<PJ301MPort>(Vec(x0, y), module, GrooveBox::GATE_BASS_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(x1, y), module, GrooveBox::BASS_OUTPUT));
		addParam(createParamCentered<RoundBlackKnob>(Vec(x3, y), module, GrooveBox::TRACK_PARAM));
		addParam(createParamCentered<VCVButton>(Vec(x4, y), module, GrooveBox::LOAD_PARAM));

		y = 126;
		addOutput(createOutputCentered<PJ301MPort>(Vec(x0, y), module, GrooveBox::GATE_MELODY_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(x1, y), module, GrooveBox::MELODY_OUTPUT));

		y = 164;
		addOutput(createOutputCentered<PJ301MPort>(Vec(x0, y), module, GrooveBox::GATE_CLAP_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(x1, y), module, GrooveBox::GATE_PERC_OUTPUT));

		y = 200;
		addOutput(createOutputCentered<PJ301MPort>(Vec(x0, y), module, GrooveBox::GATE_RIDE_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(x1, y), module, GrooveBox::GATE_CRASH_OUTPUT));

		y = 236; 
		addOutput(createOutputCentered<PJ301MPort>(Vec(x0, y), module, GrooveBox::GATE_HIHAT_OPEN_OUTPUT));

		y = 272;
		addOutput(createOutputCentered<PJ301MPort>(Vec(x0, y), module, GrooveBox::GATE_HIHAT_CLOSED1_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(x1, y), module, GrooveBox::GATE_HIHAT_CLOSED2_OUTPUT));

		y = 308;
		addOutput(createOutputCentered<PJ301MPort>(Vec(x0, y), module, GrooveBox::GATE_SNARE1_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(x1, y), module, GrooveBox::GATE_SNARE2_OUTPUT));

		y = 344;
		addOutput(createOutputCentered<PJ301MPort>(Vec(x0, y), module, GrooveBox::GATE_KICK_OUTPUT));

		y = 90;
		int x = 180;
        	JumpButton *jumpButton = createWidget<JumpButton>(Vec(x, y));
		jumpButton->module = module;
        	addChild(jumpButton);

		labelInfo = createWidget<Label>(Vec(200, 40));
		labelInfo->box.size = Vec(500, 20);
		labelInfo->text = "Testing a long string";
		labelInfo->color = metallic4;
		addChild(labelInfo);

		y += jumpButton->getHeight() + 10;

        	SectionButton *melodyButton = createWidget<SectionButton>(Vec(x, y));
		melodyButton->module = module;
		melodyButton->config_idx = 1;
		melodyButton->show_doubles = true;
        	addChild(melodyButton);

		y += melodyButton->getHeight() + 10;

        	SectionButton *sectionButton = createWidget<SectionButton>(Vec(x, y));
		sectionButton->module = module;
		sectionButton->show_doubles = true;
        	addChild(sectionButton);

		y += sectionButton->getHeight() + 10;

        	BinButton *binButton= createWidget<BinButton>(Vec(x - 20, y));
		binButton->module = module;
        	addChild(binButton);

        	GridButton *gridButton = createWidget<GridButton>(Vec(x, y));
		gridButton->module = module;
        	addChild(gridButton);

	}

	// color scheme Echo Icon Theme Palette in Inkscape
	NVGcolor metallic2 = nvgRGBA(158, 171, 176, 255);
	NVGcolor metallic4 = nvgRGBA(14, 35, 46, 255);
	NVGcolor green1 = nvgRGBA(204, 255, 66, 255);
	NVGcolor red1 = nvgRGBA(255, 65, 65, 255);

	void step() override {
		GrooveBox * module = dynamic_cast<GrooveBox*>(this->module);
		ModuleWidget::step();
		char buf[256];
		if (module) {
			snprintf(buf, sizeof(buf), "g: %i m: %i", module->groove_idx, module->melody_idx);
			labelInfo->text = buf;
		}
	}
};

Model* modelGrooveBox = createModel<GrooveBox, GrooveBoxWidget>("groovebox");
