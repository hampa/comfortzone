#include <math.h>
#include "plugin.hpp"
#define PI_F 3.1415927410125732421875f
#define TWOPI_F (2.0f * PI_F)
#define HALFPI_F (PI_F * 0.5f)

class ModernOsc {
	public:
		ModernOsc() {}
		~ModernOsc() {}

		enum {
			WAVE_SIN,
			WAVE_TRI,
			WAVE_SAW,
			WAVE_RAMP,
			WAVE_LAST,
		};

		void Init(float sample_rate) {
			sr = sample_rate;
			sr_recip = 1.0f / sample_rate;
			freq = 100.0f;
			amp = 1.0f;
			phase01 = 0.0f;
			delta = CalcPhaseInc(freq_);
			waveform = WAVE_SIN;
			eoc = true;
			eor = true;
			detune = 0;
		}

		inline float GetPhase01() {
			return phase;
		}

		inline float GetPhaseInc() {
			return delta;
		}

		inline void SetFreq(const float f) {
			freq = f;
			delta = CalcPhaseInc(f);
		}

		inline void SetTZFM(const float pitch, const float modulator, const float amount, const float detune) {
			float fm_freq = dsp::FREQ_C4 * std::pow(2.f, pitch + 30.f) / std::pow(2.f, 30.f);
			fm_freq += dsp::FREQ_C4 * modulator * 5.0f * amount;
			detune = detune;
			SetFreq(fm_freq);
		}

		inline void SetAmp(const float a) { amp = a; }

		inline void SetWaveform(const int wf) {
			waveform = wf < WAVE_LAST ? wf : WAVE_SIN;
		}

		inline bool IsEOR() { return eor; }
		inline bool IsEOC() { return eoc; }
		inline bool IsRising() { return phase01 < 0.5f; }
		inline bool IsFalling() { return phase01 >= 0.5f; }
		float Process();
		float GetSaw();
		void PhaseAdd(float phase) { phase01 += phase; }
		void Reset(float phase = 0.0f) { phase01 = phase; }

		int GetWaveform() { return waveform; }
	private:
		float CalcPhaseInc(float f);
		int waveform_;
		float amp, freq;
		float sr, sr_recip, phase01, delta;
		float detune_;
		bool eor, eoc;
};
