#ifndef __QSB_H
#define __QSB_H

#include <cstddef>
#include <vector>
#include <complex>
#include "random.h"
#include "audioprocess.h"

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
	MovAvg *av1;
	MovAvg *av2;
	MovAvg *av3;
	std::vector<double> gain_buf;
	size_t gain_bufsize;
	size_t gain_bufptr;
	double norm;
	double gain0;  // Previous gain value for interpolation
	size_t navg;

	// Pre-allocated buffers for newBuf() filtering stages
	std::vector<std::complex<double>> _r, _stage1, _stage2;
};

#endif
