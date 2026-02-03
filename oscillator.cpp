
#include <unistd.h>
#include "oscillator.h"


constexpr double pi = 3.14159265358979323846;

#include "wavetables.cpp"

Oscillator::Oscillator()
{
	tableIndex = 0.0;
	tableDelta = 0.0;

	wave = wavetable_sinusoidal;
}

void Oscillator::setFrequency(double frequency, double sampleRate)
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

double Oscillator::nextSample()
{
	unsigned int i0 = (unsigned int) tableIndex;
	unsigned int i1 = (i0 == wavetable_size - 1) ? 0 : i0 + 1;
	double frac = tableIndex - (double) i0;
	double res;

	res = wave[i0] + frac * (wave[i1] - wave[i0]);
	tableIndex += tableDelta;

	if (tableIndex > wavetable_size)
		tableIndex -= (double) wavetable_size;

	return res;
}






void AudioContent::addOscillator(double frequency, int wform)
{
	Oscillator *osc = new Oscillator();

	osc->setFrequency(frequency, sampleRate);
	osc->setWaveform(wform);

	l.push_back(osc);
}

double AudioContent::nextSample()
{
	double r = 0.0;

	for (auto i : l) {
		r += i->nextSample();
	}

	return r / (double) l.size();
}
