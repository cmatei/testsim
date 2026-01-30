#include "qsb.h"
#include <cmath>
#include <algorithm>
#include <cassert>

// QSB implementation
QSB::QSB(RNG *rng_, float bandwidth_, size_t bufsize_, size_t rate_)
	: rng(rng_), bandwidth(0.0), bufsize(bufsize_), rate(rate_),
	  av1(nullptr), av2(nullptr), av3(nullptr),
	  gain_bufptr(0), gain0(1.0)
{
	buf4 = bufsize / 4;
	setBandwidth(bandwidth_);
}

QSB::~QSB()
{
	delete av1;
	delete av2;
	delete av3;
}

void QSB::setBandwidth(float bw)
{
	bandwidth = bw;

	// Calculate navg (number of samples to average)
	navg = std::max(static_cast<size_t>(std::ceil(0.37 * rate / (buf4 * bandwidth))),
	                static_cast<size_t>(1));

	// Normalization factor
	norm = std::sqrt(3.0 * navg);

	// Calculate gain buffer size (must be multiple of 4, minimum 100)
	gain_bufsize = std::max(navg + 4 - (navg % 4), static_cast<size_t>(100));

	// Create or recreate MovAvg filters
	delete av1;
	delete av2;
	delete av3;
	av1 = new MovAvg(gain_bufsize, navg);
	av2 = new MovAvg(gain_bufsize, navg);
	av3 = new MovAvg(gain_bufsize, navg);

	// Initialize gain buffer
	gain_buf.clear();
	gain_buf.reserve(gain_bufsize);

	// Initialize: need to advance past 3*navg samples to get stable output
	size_t bufptr = 3 * navg;
	newBuf();

	while (gain_bufsize < bufptr) {
		newBuf();
		bufptr -= gain_bufsize;
	}

	gain0 = gain_buf[bufptr];
	gain_bufptr = bufptr + 1;

	if (gain_bufptr >= gain_bufsize) {
		newBuf();
	}
}

void QSB::newBuf()
{
	// Generate random complex numbers with uniform distribution in [-1, 1] + i[-1, 1]
	std::vector<std::complex<double>> r;
	r.reserve(gain_bufsize);

	for (size_t i = 0; i < gain_bufsize; i++) {
		double real = 2.0 * rng->uniform() - 1.0;
		double imag = 2.0 * rng->uniform() - 1.0;
		r.push_back(std::complex<double>(real, imag));
	}

	// Apply three stages of moving average filtering
	auto stage1 = av1->avg(r);
	auto stage2 = av2->avg(stage1);
	auto stage3 = av3->avg(stage2);

	// Take absolute value and normalize
	gain_buf.clear();
	for (const auto &c : stage3) {
		gain_buf.push_back(std::abs(c) * norm);
	}

	gain_bufptr = 0;
}

void QSB::applyTo(std::vector<float> &buf)
{
	// Apply gain with linear interpolation across 4 segments
	for (size_t i = 0; i < 4; i++) {
		double g1 = gain_buf[gain_bufptr];

		// Apply linear interpolation from gain0 to g1 across this segment
		size_t start = i * buf4;
		size_t end = (i + 1) * buf4;

		for (size_t j = start; j < end; j++) {
			double t = static_cast<double>(j - start) / buf4;
			double gain = gain0 + t * (g1 - gain0);
			buf[j] *= gain;
		}

		gain0 = g1;
		gain_bufptr++;

		if (gain_bufptr >= gain_bufsize) {
			newBuf();
		}
	}
}
