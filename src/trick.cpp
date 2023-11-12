#include "plugin.hpp"
#include "oscillator.h"
#include <math.h>
//#include "onetwo.h"
//#include "inverse_saw3.h"
//#include "inverse_offset01.h"
//#include "inverse_onetwotree.h"
//#include "inverse_step2.h"
//#include "inverse_cryobass.h"
#include "inverse_freq.h"
#include "inverse_vca.h"

#define LERP(a, b, f) (a * (1.0f - f)) + (b * f)
#define NUM_OSC 76
#define CLAMP(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

struct Trick : Module {
	enum ParamIds {
		FM_FREQ_PARAM,
		AMOUNT_PARAM,
		WAVETABLE_SCAN_PARAM,
		REFERENCE_FREQ_PARAM,
		//PHASE_OFFSET_PARAM,
		NUM_PARAMS
	};

	enum InputIds {
		SOURCE_INPUT,
		TARGET_INPUT,
		RESTART_INPUT,
		PING_INPUT,
		NUM_INPUTS
	};

	enum OutputIds {
		RESET_OUTPUT,
		CV_OUTPUT,
		TARGET_OUTPUT,
		DIFF_OUTPUT,
		PONG_OUTPUT,
		VCA_OUTPUT,
		NUM_OUTPUTS
	};

	enum LightIds {
		LIGHT0_LIGHT,
		LIGHT1_LIGHT,
		NUM_LIGHTS
	};

	Oscillator osc[NUM_OSC];
	Oscillator oscFm;
	float partials[NUM_OSC];

	Trick () {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(WAVETABLE_SCAN_PARAM, 0.f, 1.f, 0.5f, "Wavetable Scan", "%", 0.f, 100.f);
		configParam(FM_FREQ_PARAM, 0.f, 1.f, 0.5f, "FM Freq", "%", 0.f, 100.f);
		configParam(AMOUNT_PARAM, 0.f, 1.f, 1.0f, "FM Amount", "%", 0.f, 100.f);
		configParam(REFERENCE_FREQ_PARAM, 1.f, 20000.f, 2.0f, "Reference Freq");
		//configParam(PHASE_OFFSET, 0.f, 3.14f, 0.0f, "Phase Offset");
		configInput(PING_INPUT, "Pong");
		configInput(SOURCE_INPUT, "Source");
		configInput(TARGET_INPUT, "Target");
		configInput(RESTART_INPUT, "Restart");
	}

	~Trick () {
	}

	const char *getNoteName(int i) {
		const char *notes[] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "G#", "A", "Bb", "B" };
		return notes[(i + 12) % 12];
	}

	int fmNote = 0;
	const char *getRootNoteName() {
		return getNoteName(fmNote);
	}

	float phaseModulator = 0;
	float reset_value;
	float prev_reset_value;
	float phaseCarrier = 0;
	float prev_center = 0;
	float prev_left = 0;
	float smallest_diff;
	float best_cv;
	float best_current;
	int ping_idx;
	float ping;
	#define BUFFER_SIZE 10600 
	struct buffer_t {
		int id;
		float cv;
		float in;
		float diff;
		float current;
		float expected_input;
		float target;
	};
	struct buffer_t buffer[BUFFER_SIZE];


	int wavetable_idx = 0;
#define NUM_WAVETABLES 16
	float wavetableProcess[33];
	float sample_rate = 48000;
	int total_play_idx;
	Oscillator wtOsc[NUM_WAVETABLES][3];
	int wavetables[NUM_WAVETABLES][3] = {
		{1, 5,  8}, // 0
		{1, 7, 10}, // 1
		{1, 7, 16}, // 2
		{1,11, 19}, // 3
		{1,14, 18}, // 4
		{1,13, 18}, // 5
		{1,17, 22}, // 6
		{1,16, 24}, // 7
		{1, 7, 18}, // 8
		{1,10, 18}, // 9
		{1,18, 21}, // 10
		{1,12, 22}, // 11
		{1,19, 25}, // 12
		{1, 8, 21}, // 13
		{1, 8, 30}, // 14
		{1,21, 32}  // 15
	};

	void onSampleRateChange(const SampleRateChangeEvent& e) override {
		for (int i = 0; i < NUM_OSC; i++) {
			osc[i].Init(e.sampleRate);
			osc[i].SetWaveform(Oscillator::WAVE_SIN);
			partials[i] = 0;
		}
		oscFm.Init(e.sampleRate);
		oscFm.SetWaveform(Oscillator::WAVE_SIN);
		oscFm.SetFreq(440);
		sample_rate = e.sampleRate;
		reset_buffer();
	}

	void train_neural_network(float *weights, float x[], float y[], int num_samples, int num_epochs, float learning_rate) {
		srand(time(NULL));

		// Initialize the weights randomly
		for (int i = 0; i < 2; i++) {
			weights[i] = (float)rand() / (float)(RAND_MAX / 2) - 1.0;
		}

		// Train the neural network using stochastic gradient descent
		for (int epoch = 0; epoch < num_epochs; epoch++) {
			for (int i = 0; i < num_samples; i++) {
				// Forward pass
				float y_pred = predict_neural_network(weights, x[i]);

				// Compute error
				float error = y[i] - y_pred;

				// Update weights using gradient descent
				weights[0] += learning_rate * error * x[i];
				weights[1] += learning_rate * error;
			}
		}
	}

	inline float fmap_linear(float in, float min, float max) {
		return CLAMP(min + in * (max - min), min, max);
	}

	float predict_neural_network(float *weights, float x) {
        	return weights[0] * x + weights[1];
	}

	inline float mtof(float m) {
		return powf(2, (m - 69.0f) / 12.0f) * 440.0f;
	}

	/*
	   inline int getOffset(float f) {
	   int idx = floorf(f) % NUM_OSC;
	   }
	 */

	inline float getPartial(float f) {
		int idxA = floorf(f);
		idxA %= NUM_OSC;
		int idxB = idxA + 1;
		float rem = f - floorf(f);
		if (idxA < 0) {
			idxA = NUM_OSC - 1;
			idxB = 0;
		}
		if (idxB > NUM_OSC) {
			idxB = 0;
		}
		float result = LERP(partials[idxA], partials[idxB], rem);
		return result;
	}

	inline float calculate_frequency_adjustment(float sample_rate, float current_value, float target_value, float current_frequency) {
		float phase_diff = acosf((current_value * target_value + sqrtf(1 - powf(current_value, 2)) * sqrtf(1 - powf(target_value, 2))) / (sqrtf(powf(current_value, 2) + powf(sqrtf(1 - powf(current_value, 2)), 2)) * sqrtf(powf(target_value, 2) + powf(sqrtf(1 - powf(target_value, 2)), 2))));
		float freq_adjustment = (phase_diff * sample_rate) / (2 * M_PI);
		return current_frequency + freq_adjustment;
	}

	inline float frequency_to_cv(float frequency) {
		float base_frequency = 20;
		float frequency_ratio = frequency / base_frequency;
		float num_octaves = log2f(frequency_ratio);
		float cv = num_octaves;
		return cv;
	}


	bool restart;
	int length;
	int lag_idx = 0;
	float cv;
	float best;

	enum {
		REC_REPLAY,
		REC_PLAY,
		REC_RESET,
		REC_MEASURE,
		REC_MEASURE_WAIT,
		REC_MEASURE_WAIT2,
		REC_RESET_WAIT,
		REC_TRAIN
	};
	int rec_state = 0;
	int rec_idx = 0;
	int reset_idx = 0;
	float current_test_cv = 0;
	int play_idx;
	int got_reset = 0;
	int time_idx = 0;
	float diff_inc = 0.1f;
	dsp::SchmittTrigger restartTrig;
	int sample_idx = 0;
	int measure_idx = 0;
	void reset_buffer() {
		smallest_diff = 10000;
		measure_idx = 0;
		current_test_cv = 0;
		float f = 0.0f;
		for (int i = 0; i < BUFFER_SIZE; i++) {
			buffer[i].in = 0;
			buffer[i].diff = 0;
			buffer[i].cv = 0;
			//buffer[i].target = i / (float)BUFFER_SIZE;
			//buffer[i].target = f; // / (float)BUFFER_SIZE;
			//buffer[i].target = rawData[i] * 2.0f;
			f += 0.1f;
			if (f > 2) {
				f -= 2.0f;
			}
		}
	}

	float prev_lowpass_output = 0.0f;
	void lowpass_reset() {
		prev_lowpass_output = 0.0f;
	}

	float lowpass_filter(float input, float cutoff_freq) {
		float dt = 1.0f / (float)sample_rate;
		float RC = 1.0f / (2.0f * M_PI * cutoff_freq);
		float alpha = dt / (dt + RC);
		float output = alpha * input + (1 - alpha) * prev_lowpass_output;
		prev_lowpass_output = output;
		return output;
	}

	float freq_to_cv(float frequency, float reference_frequency = 2) {
		//float reference_frequency = 2; // Middle C (C4)
		if (frequency <= 0) {
			return -1;
		}
		float cv = log2f(frequency / reference_frequency);
		return cv;
	}

	void process(const ProcessArgs& args) override {
		float in = inputs[SOURCE_INPUT].getVoltage();
		float ref_freq = params[REFERENCE_FREQ_PARAM].getValue();
		float amount = params[AMOUNT_PARAM].getValue();

		outputs[RESET_OUTPUT].setVoltage(5.0f * (sample_idx < 0)); 
		float out = 0;
		if (sample_idx >= 0) {
			lowpass_reset();
			out = freq_to_cv(inverse_freq[sample_idx], ref_freq);
		}
		if (amount < 0.9f) {
			float cutoff = fmap_linear(amount, 20, sample_rate);
			out = lowpass_filter(out, cutoff);
		}
		outputs[CV_OUTPUT].setVoltage(out);
		outputs[VCA_OUTPUT].setVoltage(inverse_vca[sample_idx] * 4.0f);
		sample_idx++;
		if (sample_idx > INVERSE_FREQ_SIZE) {
			sample_idx = -10;
		}	

		/*
		// deterministic check
		if (ping == 0) {

		}
		inputs[PING_INPUT].getVoltage();
		outputs[PONG_OUTPUT].setVoltage(ping);
		ping_idx++;
		if (ping_idx > 30) {
			ping_idx = 0;
			ping = 1;
		}
		*/
		//DEBUG("send_ping %f got_ping %f", ping, pong_value); 
	}

	void process3(const ProcessArgs& args) {
		float in = inputs[SOURCE_INPUT].getVoltage();

		bool reset = restartTrig.process(inputs[RESTART_INPUT].getVoltage());
		if (reset) {
			reset_buffer();
		}
		// something we do at sample_idx can be measured at write_idx

		int write_idx = sample_idx - 3;
		if (write_idx < 0) {
			write_idx += BUFFER_SIZE;
		}

		int measure_write_idx = measure_idx - 3;
		if (measure_write_idx < 0) {
			measure_write_idx += BUFFER_SIZE;
		}

		buffer[write_idx].in = in;
		//buffer[write_idx].target = target;
		//buffer[write_idx].diff = diff; 
		//DEBUG("sample_idx %i write_idx %i in: %.5f outcv: %.5f ", sample_idx, write_idx, in, outcv); 
		//DEBUG("sample_idx %i write_idx %i in: %.5f outcv: %.5f ", sample_idx, write_idx, in, outcv); 

		outputs[RESET_OUTPUT].setVoltage(5.0f * (sample_idx == 0)); 
		outputs[CV_OUTPUT].setVoltage(buffer[sample_idx].cv);

		sample_idx++;
		if (sample_idx >= BUFFER_SIZE) {
			sample_idx = 0;
			if (measure_idx < BUFFER_SIZE) {
				float target = buffer[measure_idx].target;
				float diff = fabs(target - buffer[measure_write_idx].in);
				if (diff < smallest_diff) {
					smallest_diff = diff;
					best_cv = current_test_cv;
				}
				buffer[measure_write_idx].cv = current_test_cv;
				current_test_cv += 0.01f;
				if (current_test_cv > 15) {
					buffer[measure_write_idx].diff = smallest_diff;
					buffer[measure_write_idx].cv = best_cv;
					measure_idx++;
					current_test_cv = 0;
					smallest_diff = 50;
					best_cv = 0;
					if (measure_idx >= BUFFER_SIZE) {
						for (int i = 0; i < BUFFER_SIZE; i++) {
							DEBUG("XXXDONE %i target: %.5f in: %.5f diff: %.5f cv: %.5f", 
									i, buffer[i].target, buffer[i].in, buffer[i].diff, buffer[i].cv);
						}
					}
					DEBUG("XXXPROG measure_idx: %i current_test_cv: %.5f best_cv: %.5f smallest_diff: %.5f", measure_idx, current_test_cv, best_cv, smallest_diff);
				}
				/*
				else {
					for (int i = 0; i < BUFFER_SIZE; i++) {
						DEBUG("XXXPROG %i target: %.5f in: %.5f diff: %.5f cv: %.5f", 
								i, buffer[i].target, buffer[i].in, buffer[i].diff, buffer[i].cv);
					}

				}	
				*/
			}
			//DEBUG("write_idx %i in: %.5f current_test_cv: %.5f smallest_diff", write_idx, in, current_test_cv, smallest_diff); 
		}

		//outputs[PONG_OUTPUT].setVoltage(ping);
		//outputs[CV_OUTPUT].setVoltage(out_cv);
		//outputs[TARGET_OUTPUT].setVoltage(target_out);
		//outputs[DIFF_OUTPUT].setVoltage(diff_out);
		//buffer[rec_idx].cv = best_cv;	
		//buffer[rec_idx].diff = smallest_diff;	
	}

	void process2(const ProcessArgs& args) {
		//float phaserWidth = params[PHASER_WIDTH_PARAM].getValue();
		//float phaserOffsetInput = params[PHASER_OFFSET_PARAM].getValue();
		//float wavetableScan = params[WAVETABLE_SCAN_PARAM].getValue();
		//float fmAmount = params[FM_AMOUNT_PARAM].getValue();
		//float fmMidi = params[FM_FREQ_PARAM].getValue() * 127.0f - 64.0f;
		//float fmFreq = mtof(fmMidi);
		bool reset = restartTrig.process(inputs[RESTART_INPUT].getVoltage());
		if (reset) {
			play_idx = rec_idx = 0;
			rec_state = REC_REPLAY;
			reset_idx = 3; // 3 turns lag
			smallest_diff = 10000;
			DEBUG("RESET play"); 
		}

		float current_value = inputs[SOURCE_INPUT].getVoltage();
		//float pong_value = inputs[PING_INPUT].getVoltage();
		float target_out = 0;
		int reset_is_on = (--reset_idx > 0);
		/*	
		float lag_volt = 4;
		if (lag_idx < 10) {
			lag_volt = 0;
		}
		outputs[CV_OUTPUT].setVoltage(lag_volt);
		lag_idx++;
		if (lag_idx > 30) {
			lag_idx = 0;
		}
		*/
		ping_idx++;
		ping = 0;
		if (ping_idx > 30) {
			ping_idx = 0;
			ping = 1;
		}
		outputs[PONG_OUTPUT].setVoltage(ping);
		//DEBUG("send_ping %f got_ping %f", ping, pong_value); 
	
		/*
		if (reset_idx > -5) {
			DEBUG("reset_idx %i %f reset: %i", reset_idx, current_value, reset_is_on);
			got_reset = false;
		}
		*/
		outputs[RESET_OUTPUT].setVoltage(5.0f * reset_is_on); 
		//DEBUG("rec_state %i rec_idx %i play_idx %i current_value %f", rec_state, rec_idx, play_idx, current_value);
		if (rec_state == REC_RESET) {
			DEBUG("reset idx %i current_value %f", reset_idx, current_value);
			if (reset_idx <= 0) {
				rec_state = REC_REPLAY;
				play_idx = 0;
				reset_idx = 0;
				DEBUG("AAAA %i Using start value %f", time_idx, current_value);
			}
		}
		/*
		if (rec_state == REC_TRAIN) {
		        // Define the input and output data
        		float x[] = {-5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5};
        		float y[] = {-4.5, -3.5, -2.5, -1.5, -0.5, 0.5, 1.5, 2.5, 3.5, 4.5, 5.5}; // Example of a system with a linear relationship
        		int num_samples = sizeof(x) / sizeof(x[0]);

        		nt num_epochs = 1000;
        		float learning_rate = 0.001;
        		float weights[2];
			train_neural_network(weights, x, y, BUFFER_SIZE, num_epochs, learning_rate);
		}
		*/


		if (rec_state == REC_RESET_WAIT) {
			//DEBUG("AAAB reset_idx %i on: %i value %f ", reset_idx, reset_is_on, current_value);
			if (reset_idx <= 0) {
				//DEBUG("AAAB reset_idx %i on: %i start value %f", reset_idx, reset_is_on, current_value);
				reset_value = current_value;
				if (reset_value != prev_reset_value) {
					DEBUG("BB new reset %f old %f", reset_value, prev_reset_value);
					prev_reset_value = reset_value;
				}
				/*
				else {
					DEBUG("BB reset stable %f", reset_value);
				}
				*/
				rec_state = REC_PLAY;
				play_idx = 0;
				reset_idx = 0;
			}
		}

		//float adjust = calculate_frequency_adjustment(sample_rate, current_value, target_value, 440.0);
		//float adjust_cv = frequency_to_cv(adjust, 440.0f);
		float out_cv = 0;
		float diff_out = 0;
		if (rec_state == REC_REPLAY) {
			if (play_idx < rec_idx) {
				out_cv = play_idx; //buffer[play_idx].cv;
				DEBUG("EEE rec_idx: %i play_idx: %i current_value: %f out_cv: %f", rec_idx, play_idx, current_value, out_cv);

			}
			else if (play_idx == rec_idx) {
				out_cv = play_idx; //current_test_cv;
				DEBUG("EEE rec_idx: %i current_test_cv: %f current_value: %f", rec_idx, current_test_cv, current_value);
				//rec_state = REC_MEASURE_WAIT;
				rec_state = REC_MEASURE_WAIT;
			}
			//DEBUG("REC_REPLAY play %i rec %i out_cv %f", play_idx, rec_idx, out_cv);
			play_idx++;
		}
		else if (rec_state == REC_MEASURE_WAIT) {
			DEBUG("EEE REC_MEASURE_WAIT for current_value: %f", current_value);
			//rec_state = REC_MEASURE_WAIT2;
			rec_state = REC_MEASURE;
		}
		else if (rec_state == REC_MEASURE_WAIT2) {
			DEBUG("EEE REC_MEASURE_WAIT2 current_value: %f", current_value);
			rec_state = REC_MEASURE;
		}
		else if (rec_state == REC_MEASURE) {
			float diff = fabs(buffer[rec_idx].target - current_value);	
			DEBUG("EEE REC_MEASURE rec_idx: %i current_test_cv: %f current_value: %f best_cv: %f diff: %f smallest_dist: %f target: %f", rec_idx, current_test_cv, current_value, best_cv, diff, smallest_diff, buffer[rec_idx].target);
			if (diff < smallest_diff) {
				smallest_diff = diff;
				best_cv = current_test_cv;
				best_current = current_value;
				DEBUG("DDD rec_idx: %i current_value: %f best_cv: %f diff: %f best_current: %f target: %f", rec_idx, current_value, best_cv, smallest_diff, best_current, buffer[rec_idx].target);
			}
			/*
			if (diff < 0.01f) {
				current_test_cv += 0.001f;
			}
			else if (diff < 0.1) {
				current_test_cv += 0.01f;
			}
			else {
				current_test_cv += 0.1f;
			}
			*/
			rec_state = REC_PLAY;
			//current_test_cv += 0.005f;
			current_test_cv = rec_idx;
			buffer[rec_idx].cv = best_cv;	
			buffer[rec_idx].diff = smallest_diff;	
			rec_idx++;
			if (rec_idx >= BUFFER_SIZE) {
				rec_state = REC_PLAY;
			}
			else {
				rec_state = REC_RESET;
				reset_idx = 3;
			}

			if (current_test_cv > 10) {
				current_test_cv = 0;
				buffer[rec_idx].cv = best_cv;	
				buffer[rec_idx].diff = smallest_diff;	
				if (smallest_diff < 0.3) {
					buffer[rec_idx].current = best_current;	
					DEBUG("YYY rec_idx: %i PASS best_cv: %f diff: %f best_current: %f target: %f", rec_idx, best_cv, smallest_diff, best_current, buffer[rec_idx].target);
				}
				else {
					buffer[rec_idx].current = 10;
					DEBUG("YYY rec_idx: %i FAIL best_cv: %f diff: %f best_current: %f target: %f", rec_idx, best_cv, smallest_diff, best_current, buffer[rec_idx].target);
				}
				rec_idx++;
				smallest_diff = 100;
				if (rec_idx >= BUFFER_SIZE) {
					rec_state = REC_PLAY;
				}
				else {
					rec_state = REC_RESET;
					reset_idx = 3;
				}
				play_idx = 0;
			}
		}
		else if (rec_state == REC_PLAY) {
			out_cv = buffer[play_idx].cv;
			//DEBUG("ZZZ %i cv %f diff %f buffer_current %f target %f in %f", play_idx, buffer[play_idx].cv, buffer[play_idx].diff, buffer[play_idx].current, buffer[play_idx].target, current_value); 
			target_out = buffer[play_idx].target;
			diff_out = buffer[play_idx].diff * 10;
			play_idx++;
			if (play_idx >= BUFFER_SIZE) {
				play_idx = 0;
				//reset_idx = 3;
				//rec_state = REC_RESET_WAIT;
			}
		}
			
		//float adjust_cv = frequency_to_cv(out_cv);
		outputs[PONG_OUTPUT].setVoltage(ping);
		outputs[CV_OUTPUT].setVoltage(out_cv);
		outputs[TARGET_OUTPUT].setVoltage(target_out);
		outputs[DIFF_OUTPUT].setVoltage(diff_out);
		outputs[RESET_OUTPUT].setVoltage(5.0f * reset_is_on);
		time_idx++;
	}
};

struct TrickWidget : ModuleWidget {
	Label* labelPitch;

	TrickWidget(Trick* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/tzfmlead5HP.svg")));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));

		float spacingX = box.size.x / (float)4.0f;
		int x0 = spacingX;
		int x1 = box.size.x - spacingX;

		float y = 72;
		addParam(createParamCentered<RoundBlackKnob>(Vec(x0, y), module, Trick::REFERENCE_FREQ_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(Vec(x1, y), module, Trick::AMOUNT_PARAM));

		y = 142;
		addInput(createInputCentered<PJ301MPort>(Vec(x0, y), module, Trick::PING_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(x1, y), module, Trick::PONG_OUTPUT));

		y = 190;
		addInput(createInputCentered<PJ301MPort>(Vec(x0, y), module, Trick::RESTART_INPUT));
		//addParam(createParamCentered<RoundBlackKnob>(Vec(x0, y), module, Trick::FM_FREQ_PARAM));
		//addParam(createParamCentered<RoundBlackKnob>(Vec(x1, y), module, Trick::FM_AMOUNT_PARAM));

		y = 238;
		addInput(createInputCentered<PJ301MPort>(Vec(x0, y), module, Trick::SOURCE_INPUT));
		addInput(createInputCentered<PJ301MPort>(Vec(x1, y), module, Trick::TARGET_INPUT));
		//addParam(createParamCentered<RoundBlackKnob>(Vec(x0, y), module, Trick::WAVETABLE_SCAN_PARAM));
		//addParam(createParamCentered<RoundBlackKnob>(Vec(x1, y), module, Trick::CARRIER_OCTAVE_PARAM));

		y = 296;
		//addOutput(createOutputCentered<PJ301MPort>(Vec(x0, y), module, Trick::TARGET_OUTPUT));
		//addOutput(createOutputCentered<PJ301MPort>(Vec(x1, y), module, Trick::DIFF_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(x0, y), module, Trick::VCA_OUTPUT));

		y = 344;
		addOutput(createOutputCentered<PJ301MPort>(Vec(x0, y), module, Trick::RESET_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(x1, y), module, Trick::CV_OUTPUT));

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
		Trick * module = dynamic_cast<Trick*>(this->module);
		if (module) {
			labelPitch->text = module->getRootNoteName();
		}
		ModuleWidget::step();
	}
};

Model* modelTrick = createModel<Trick, TrickWidget>("trick");
