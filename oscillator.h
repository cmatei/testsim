#ifndef __OSCILLATOR_H
#define __OSCILLATOR_H

#include <stdint.h>

#include <list>
#include <random>

class Oscillator {

public:
	Oscillator();

public:
	void setFrequency(double frequency, double sampleRate);
	void setWaveform(int wform);

	double nextSample();

	static const int WAVE_SINUSOIDAL = 1;
	static const int WAVE_TRIANGULAR = 2;
	static const int WAVE_SAWTOOTH = 3;
	static const int WAVE_SQUARE = 4;

private:
	double tableIndex;
	double tableDelta;

	double *wave;
};

class AudioContent {
public:
	AudioContent(double sampleRate = 48000.0) {
		this->sampleRate = sampleRate;
	}

	double nextSample();

	void addOscillator(double frequency, int wform = Oscillator::WAVE_SINUSOIDAL);

private:
	double sampleRate;
	std::list<Oscillator *> l;
};


#endif
