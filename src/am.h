#include <math.h>
#include "plugin.hpp"
#define PI_F 3.1415927410125732421875f
#define TWOPI_F (2.0f * PI_F)
#define HALFPI_F (PI_F * 0.5f)

class AM {
	public:
		AM() {}
		~AM() {}

		enum {
			WAVE_FULL,
			WAVE_TRI,
			WAVE_UP,
			WAVE_DOWN,
			WAVE_LAST,
		};

		void Init(float sample_rate) {
			sr_ = sample_rate;
			sr_recip_ = 1.0f / sample_rate;
			freq_ = 100.0f;
			amp_ = 1.0f;
			phase_ = 0.0f;
			phase_inc_ = CalcPhaseInc(freq_);
			waveform_ = WAVE_TRI;
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

		inline void SetAmp(const float a) { amp_ = a; }

		inline void SetWaveform(const int wf) {
			waveform_ = wf < WAVE_LAST ? wf : WAVE_TRI;
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
		float detune_;
		bool eor_, eoc_;
};
