#include <math.h>
#include "plugin.hpp"
#define PI_F 3.1415927410125732421875f
#define TWOPI_F (2.0f * PI_F)
#define HALFPI_F (PI_F * 0.5f)

class Oscillator {
	public:
		Oscillator() {}
		~Oscillator() {}

		enum {
			WAVE_SIN,
			WAVE_TRI,
			WAVE_SAW,
			WAVE_RAMP,
			WAVE_SQUARE,
			WAVE_POLYBLEP_SAW,
			WAVE_POLYBLEP_SQUARE,
			WAVE_LAST,
		};

		void Init(float sample_rate) {
			sr_ = sample_rate;
			sr_recip_ = 1.0f / sample_rate;
			freq_ = 100.0f;
			amp_ = 1.0f;
			phase_ = 0.0f;
			phase_inc_ = CalcPhaseInc(freq_);
			waveform_ = WAVE_SIN;
			eoc_ = true;
			eor_ = true;
			detune_ = 0;
		}

		inline float GetPhase() {
			return phase_;
		}

		inline float GetPhaseInc() {
			return phase_inc_;
		}

		inline void SetFreq(const float f) {
			freq_ = f;
			phase_inc_ = CalcPhaseInc(f);
		}

		inline void SetTZFM(const float pitch, const float modulator, const float amount, const float detune) {
			float fm_freq = dsp::FREQ_C4 * std::pow(2.f, pitch + 30.f) / std::pow(2.f, 30.f);
			fm_freq += dsp::FREQ_C4 * modulator * 5.0f * amount;
			detune_ = detune;
			SetFreq(fm_freq);
		}

		inline void SetAmp(const float a) { amp_ = a; }

		inline void SetWaveform(const int wf) {
			waveform_ = wf < WAVE_LAST ? wf : WAVE_SIN;
		}

		inline bool IsEOR() { return eor_; }

		inline bool IsEOC() { return eoc_; }

		inline bool IsRising() { return phase_ < PI_F; }

		inline bool IsFalling() { return phase_ >= PI_F; }

		float Process();

		void PhaseAdd(float _phase) { phase_ += (_phase * TWOPI_F); }

		void Reset(float _phase = 0.0f) { phase_ = _phase; }

		int GetWaveform() { return waveform_; }
	private:
		float CalcPhaseInc(float f);
		int waveform_;
		float amp_, freq_;
		float sr_, sr_recip_, phase_, phase_inc_;
		float last_out_, last_freq_;
		float detune_;
		bool eor_, eoc_;
};
