#ifndef __AUDIOPROCESS_H
#define __AUDIOPROCESS_H

#include <cstddef>
#include <vector>
#include <complex>
#include <cassert>

class MovAvg {
public:
        MovAvg(size_t bufsize, size_t navg): bufsize(bufsize), navg(navg)
	{
		assert(bufsize > navg);
		prev.resize(bufsize + navg);  // Fixed: was bufsize + navg - 1
		std::fill(prev.begin(), prev.end(), 0.0);
	}

	std::vector<double> avg(std::vector<double> buf);

private:
	size_t bufsize, navg;
	std::vector<double> prev;
};

class Modulator {
public:
        Modulator(size_t bufsize, size_t samplerate, double pitch, bool reverse = false):
	        bufsize(bufsize), samplerate(samplerate), reverse(reverse) { setPitch(pitch); }

	void setPitch(double pitch);
	void setReverse(bool rev) { reverse = rev; setPitch(pitch); }

	std::vector<double> modulate(std::vector<std::complex<double>> buf);

private:
	size_t bufsize, samplerate, shift;
	double pitch;
	bool reverse;

	std::vector<std::complex<double>> ex;
};

class Agc {
public:
	Agc(size_t bufsize, double maxout = 20000.0, double maxoutnorm = 0.67, double noiseindb = 76.0, double noiseoutdb = 76.0, size_t attacksamples = 155, size_t holdsamples = 155);

	std::vector<double> process(std::vector<double> buf);
private:

	size_t bufsize, agcmid, magbufsize, valbufsize;
	double maxoutnorm;
	double noisein, noiseout, beta;

	std::vector<double> agcshape, magbuf, valbuf;

	std::vector<std::vector<unsigned long>> ind;
};

#endif
