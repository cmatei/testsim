
#include <unistd.h>
#include "oscillator.h"


constexpr float pi = 3.14159265358979323846;

#include "wavetables.cpp"

Oscillator::Oscillator()
{
	tableIndex = 0.0;
	tableDelta = 0.0;

	wave = wavetable_sinusoidal;
}

void Oscillator::setFrequency(float frequency, float sampleRate)
{
	auto oversample = wavetable_size / sampleRate;
	tableDelta = frequency * oversample;
}

void Oscillator::setWaveform(int wform)
{
	switch (wform) {
	case WAVE_SQUARE:
		wave = wavetable_square;
		break;

	case WAVE_TRIANGULAR:
		wave = wavetable_triangular;
		break;

	case WAVE_SAWTOOTH:
		wave = wavetable_sawtooth;
		break;

	case WAVE_SINUSOIDAL:
	default:
		wave = wavetable_sinusoidal;
		break;
	}

}

float Oscillator::nextSample()
{
	unsigned int i0 = (unsigned int) tableIndex;
	unsigned int i1 = (i0 == wavetable_size - 1) ? 0 : i0 + 1;
	float frac = tableIndex - (float) i0;
	float res;

	res = wave[i0] + frac * (wave[i1] - wave[i0]);
	tableIndex += tableDelta;

	if (tableIndex > wavetable_size)
		tableIndex -= (float) wavetable_size;

	return res;
}






void AudioContent::addOscillator(float frequency, int wform)
{
	Oscillator *osc = new Oscillator();

	osc->setFrequency(frequency, sampleRate);
	osc->setWaveform(wform);

	l.push_back(osc);
}

float AudioContent::nextSample()
{
	float r = 0.0;

	for (auto i : l) {
		r += i->nextSample();
	}

	return r / (float) l.size();
}
