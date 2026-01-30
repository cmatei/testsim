#ifndef __OSCILLATOR_H
#define __OSCILLATOR_H

#include <stdint.h>

#include <list>
#include <random>

class Oscillator {

public:
	Oscillator();

public:
	void setFrequency(float frequency, float sampleRate);
	void setWaveform(int wform);

	float nextSample();

	static const int WAVE_SINUSOIDAL = 1;
	static const int WAVE_TRIANGULAR = 2;
	static const int WAVE_SAWTOOTH = 3;
	static const int WAVE_SQUARE = 4;

private:
	float tableIndex;
	float tableDelta;

	float *wave;
};

class AudioContent {
public:
	AudioContent(float sampleRate = 48000.0) {
		this->sampleRate = sampleRate;
	}

	float nextSample();

	void addOscillator(float frequency, int wform = Oscillator::WAVE_SINUSOIDAL);

private:
	float sampleRate;
	std::list<Oscillator *> l;
};


#endif
