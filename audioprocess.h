#ifndef __AUDIOPROCESS_H
#define __AUDIOPROCESS_H

#include <cstddef>
#include <vector>
#include <complex>
#include <cassert>

class MovAvg {
public:
	MovAvg(size_t bufsize, size_t navg);
	void avg(const std::complex<double> *in, std::complex<double> *out);

private:
	size_t bufsize, navg;
	std::vector<std::complex<double>> sums[2];  // Two cumsum buffers
	int inew;  // Toggle between 0 and 1
};

class Modulator {
public:
        Modulator(size_t bufsize, size_t samplerate, double pitch, bool reverse = false):
	        bufsize(bufsize), samplerate(samplerate), reverse(reverse) { setPitch(pitch); }

	void setPitch(double pitch);
	void setReverse(bool rev) { reverse = rev; setPitch(pitch); }

	void modulate(const std::complex<double> *in, double *out);

private:
	size_t bufsize, samplerate;
	double pitch;
	bool reverse;
	std::vector<std::complex<double>> ex;  // Pre-computed complex exponential
	size_t shift;  // Roll amount per buffer
};

class Agc {
public:
	Agc(size_t bufsize, double maxout = 20000.0, double maxoutnorm = 0.67, double noiseindb = 76.0, double noiseoutdb = 76.0, size_t attacksamples = 155, size_t holdsamples = 155);

	void process(const double *in, double *out);
private:

	size_t bufsize, agcmid, magbufsize, valbufsize, agcbufsize;
	double maxoutnorm;
	double noisein, noiseout, beta;

	double *agcshape, *magbuf, *valbuf, *gain;

	std::vector<std::vector<unsigned long>> ind;
};

#endif
