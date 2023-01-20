#include "oscillator.h"
#include <math.h>

static inline float Polyblep(float phase_inc, float t);

float Oscillator::Process() {
	float out, t;
	switch (waveform_) {
		case WAVE_SIN:
			out = sinf(phase_);
			break;
		case WAVE_TRI:
			t = -1.0f + (2.0f * phase_ * TWO_PI_RECIP);
			out = 2.0f * (fabsf(t) - 0.5f);
			break;
		case WAVE_SAW:
			out = -1.0f * (((phase_ * TWO_PI_RECIP * 2.0f)) - 1.0f);
			break;
		case WAVE_RAMP:
			out = ((phase_ * TWO_PI_RECIP * 2.0f)) - 1.0f;
			break;
		case WAVE_SQUARE:
			out = phase_ < PI_F ? (1.0f) : -1.0f;
			break;
		case WAVE_POLYBLEP_SAW:
			t = phase_ * TWO_PI_RECIP;
			out = (2.0f * t) - 1.0f;
			out -= Polyblep(phase_inc_, t);
			out *= -1.0f;
			break;
		case WAVE_POLYBLEP_SQUARE:
			t   = phase_ * TWO_PI_RECIP;
			out = phase_ < PI_F ? 1.0f : -1.0f;
			out += Polyblep(phase_inc_, t);
			out -= Polyblep(phase_inc_, fmodf(t + 0.5f, 1.0f));
			out *= 0.707f; // ?
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

float Oscillator::ProcessOffset(float offset) {
	float out, t;
	switch (waveform_) {
		case WAVE_SIN:
			out = sinf(phase_ * offset);
			break;
		case WAVE_TRI:
			t = -1.0f + (2.0f * phase_ * TWO_PI_RECIP);
			out = 2.0f * (fabsf(t) - 0.5f);
			break;
		case WAVE_SAW:
			out = -1.0f * (((phase_ * TWO_PI_RECIP * 2.0f)) - 1.0f);
			break;
		case WAVE_RAMP:
			out = ((phase_ * TWO_PI_RECIP * 2.0f)) - 1.0f;
			break;
		case WAVE_SQUARE:
			out = phase_ < PI_F ? (1.0f) : -1.0f;
			break;
		case WAVE_POLYBLEP_SAW:
			t = phase_ * TWO_PI_RECIP;
			out = (2.0f * t) - 1.0f;
			out -= Polyblep(phase_inc_, t);
			out *= -1.0f;
			break;
		case WAVE_POLYBLEP_SQUARE:
			t   = phase_ * TWO_PI_RECIP;
			out = phase_ < PI_F ? 1.0f : -1.0f;
			out += Polyblep(phase_inc_, t);
			out -= Polyblep(phase_inc_, fmodf(t + 0.5f, 1.0f));
			out *= 0.707f; // ?
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


void Oscillator::AddPhase(float phase01) {
	phase_ += (phase01 * TWOPI_F);
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
}

/*
float Oscillator::AliasSuppressedSaw(float phase, float delta) {
        float sawBuffer[3];
        float phases[3];

        phases[0] = phase - 2 * delta + (phase < 2 * delta);
        phases[1] = phase - delta + (phase < delta);
        phases[2] = phase;

	const float denominator = 0.25f / (delta * delta);

        for (int i = 0; i < 3; ++i) {
                float p = 2 * phases[i] - 1.0f;
                sawBuffer[i] = (p * p * p - p) / 6.0;
        }
        return (sawBuffer[0] - 2.0 * sawBuffer[1] + sawBuffer[2]) * denominator;
}
*/

float Oscillator::CalcPhaseInc(float f) {
	return (TWOPI_F * f) * sr_recip_;
}

static float Polyblep(float phase_inc, float t) {
	float dt = phase_inc * TWO_PI_RECIP;
	if (t < dt) {
		t /= dt;
		return t + t - t * t - 1.0f;
	}
	if (t > 1.0f - dt) {
		t = (t - 1.0f) / dt;
		return t * t + t + t + 1.0f;
	}
	return 0.0f;
}
