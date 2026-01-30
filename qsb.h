#ifndef __QSB_H
#define __QSB_H

#include <cstddef>
#include <vector>
#include <complex>
#include "random.h"
#include "audioprocess.h"

// MovAvg for complex numbers
class MovAvgComplex {
public:
	MovAvgComplex(size_t bufsize, size_t navg);
	std::vector<std::complex<double>> avg(const std::vector<std::complex<double>> &buf);

private:
	size_t bufsize, navg;
	std::vector<std::complex<double>> prev;
};

class QSB {
public:
	QSB(RNG *rng_, float bandwidth_ = 0.1, size_t bufsize_ = 512, size_t rate_ = 11025);
	~QSB();

	void setBandwidth(float bw);
	void applyTo(std::vector<float> &buf);

private:
	void newBuf();

	RNG *rng;
	float bandwidth;
	size_t bufsize, rate;
	size_t buf4;  // bufsize / 4

	// For gain generation
	MovAvgComplex *av1;
	MovAvgComplex *av2;
	MovAvgComplex *av3;
	std::vector<double> gain_buf;
	size_t gain_bufsize;
	size_t gain_bufptr;
	double norm;
	double gain0;  // Previous gain value for interpolation
	size_t navg;
};

#endif
