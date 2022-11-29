#include "am.h"
#include <math.h>

constexpr float TWO_PI_RECIP = 1.0f / TWOPI_F;
#define LERP(a, b, f) (a * (1.0f - f)) + (b * f)

float AM::Process() {
	float out;
	float phase01 = phase_ * TWO_PI_RECIP;
	switch (waveform_) {
		case WAVE_FULL:
			out = 0.7f;
			break;
		case WAVE_TRI:
			if (phase01 <= 0.5f) {
				out = LERP(0, 1, phase01 * 2.0f);
			}
			else {
				out = LERP(1, 0, (phase01 - 0.5f) * 2.0f);
			}
			break;
		case WAVE_UP:
			out = phase01;
			break;
		case WAVE_DOWN:
			out = 1.0f - phase01;
			break;
		default:
			out = 0.0f;
		break;
	}
	phase_ += phase_inc_ + detune_ * sr_recip_;
	if (phase_ > TWOPI_F) {
		phase_ -= TWOPI_F;
		eoc_ = true;
	}
	else if (phase_ < 0) {
		phase_ += TWOPI_F;
		eoc_ = true;
	}
	else {
		eoc_ = false;
	}
	eor_ = (phase_ - phase_inc_ < PI_F && phase_ >= PI_F);
	return out * amp_;
}

float AM::CalcPhaseInc(float f) {
	return (TWOPI_F * f) * sr_recip_;
}
