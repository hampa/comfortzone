#include "modernosc.h"
#include <math.h>

float ModernOsc::Process() {
	float out, t;
	switch (waveform_) {
		case WAVE_SIN:
			out = sinf(phase01 * PI_F);
			break;
		case WAVE_TRI:
			out = phase01;
			break;
		case WAVE_SAW:
			out = GetSaw(phase01, delta);
			break;
		case WAVE_RAMP:
			out = phase01;
			break;
		default:
			out = 0.0f;
		break;
	}
	phase01 += delta +  sr_recip;
	if (phase01 > 1.0f) {
		phase01 -= 1;
		eoc = true;
	}
	else if (phase01 < 0) {
		phase += 1;
		eoc = true;
	}
	else {
		eoc = false;
	}
	return out * amp;
}

float ModernOsc::GetSaw() {
        float sawBuffer[3];
        float phases[3];

        phases[0] = phase01 - 2 * delta + (phase01 < 2 * delta);
        phases[1] = phase01 - delta + (phase01 < delta);
        phases[2] = phase01;

	const float denominator = 0.25f / (delta * delta);

        for (int i = 0; i < 3; ++i) {
                float p = 2 * phases[i] - 1.0f;
                sawBuffer[i] = (p * p * p - p) / 6.0;
        }
        return (sawBuffer[0] - 2.0 * sawBuffer[1] + sawBuffer[2]) * denominator;
}

float ModernOsc::CalcPhaseInc(float f) {
	return f * sr_recip;
}
