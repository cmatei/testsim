#include <cmath>
#include <cassert>

#include <algorithm>

#include "audioprocess.h"

MovAvg::MovAvg(size_t bufsize_, size_t navg_)
	: bufsize(bufsize_), navg(navg_)
{
	assert(bufsize > navg);
	prev.resize(bufsize + navg);  // Fixed: was bufsize + navg - 1
	std::fill(prev.begin(), prev.end(), std::complex<double>(0.0, 0.0));
}

std::vector<std::complex<double>> MovAvg::avg(const std::vector<std::complex<double>> &buf)
{
	assert(buf.size() == bufsize);

	std::complex<double> sum(0.0, 0.0);
	std::vector<std::complex<double>> res;

	// Copy new data into prev buffer at the end
	for (size_t i = navg - 1; i < bufsize + navg - 1; i++) {
		prev[i] = buf[i - navg + 1];
	}

	// Calculate moving average
	for (size_t i = 0; i < bufsize + navg; i++) {
		sum += prev[i];
		if (i >= navg) {
			res.push_back(sum / static_cast<double>(navg));
			sum -= prev[i - navg];
		}
	}

	// Save the tail for next iteration
	for (size_t i = bufsize - navg; i < bufsize; i++) {
		prev[i - bufsize + navg] = buf[i];
	}

	return res;
}


void Modulator::setPitch(double pitch)
{
	size_t period, n, i;
	double dphi;
	std::complex<double> sign;

	this->pitch = pitch;

	period = (size_t) std::lrint(samplerate / pitch);
	shift = (size_t) std::lrint(bufsize % period);

	dphi = 2.0 * M_PI / period;

	n = bufsize - shift + period;

	ex.clear();
	ex.resize(n);

	if (reverse)
		sign = std::complex(0.0, 1.0);
	else
		sign = std::complex(0.0, -1.0);

	for (i = 0; i < n; i++) {
		ex[i] = -std::exp(sign * dphi * (1.0 * i));
	}
}


std::vector<double> Modulator::modulate(std::vector<std::complex<double>> buf)
{
	std::vector<double> res;

	assert(bufsize == buf.size());

	res.reserve(bufsize);  // Reserve space instead of resize to avoid double allocation

	for (auto i = 0; i < bufsize; i++) {
		res.push_back(std::imag(ex[i] * buf[i]));
	}

	std::rotate(std::begin(ex), std::begin(ex) + shift, std::end(ex));

	return res;
}

Agc::Agc(size_t bufsize, double maxout, double maxoutnorm, double noiseindb, double noiseoutdb, size_t attacksamples, size_t holdsamples):
	bufsize(bufsize), maxoutnorm(maxoutnorm)
{
	size_t agcbufsize;

	noisein = std::pow(10.0, 0.05 * noiseindb);
	noiseout = std::min(std::pow(10.0, 0.05 * noiseoutdb), 0.25 * maxout);

	beta = noisein / std::log(maxout / (maxout - noiseout));

	agcmid =  attacksamples + holdsamples;
	agcbufsize = 2 * agcmid + 1;

	agcshape.resize(agcbufsize);
	std::fill(agcshape.begin(), agcshape.end(), 1.0);

	for (auto i = 0; i < attacksamples; i++) {
		agcshape[i] = 0.5 - 0.5 * std::cos( M_PI * (i + 1) / (attacksamples + 1));
		agcshape[agcbufsize - i - 1] = agcshape[i];
	}

	magbufsize = agcbufsize + bufsize - 1;
	magbuf.resize(magbufsize);
	std::fill(magbuf.begin(), magbuf.end(), 0.0);

	valbufsize = agcmid + bufsize;
	valbuf.resize(valbufsize);
	std::fill(valbuf.begin(), valbuf.end(), 0.0);

	std::vector<unsigned long> xx;
	for (auto i = 0; i < magbufsize - agcbufsize + 1; i++) {
		xx.push_back(agcbufsize * i);
	}

	std::vector<unsigned long> yy;
	for (auto i = 0; i < agcbufsize; i++) {
		yy.push_back((agcbufsize + 1) * i);
	}

	for (auto i = 0; i < xx.size(); i++) {
		std::vector<unsigned long> row;
		for (auto j = 0; j < yy.size(); j++) {
			row.push_back(xx[i] + yy[j]);
		}

		ind.push_back(row);
	}
}


std::vector<double> Agc::process(std::vector<double> buf)
{
	std::vector<double> res;

	// Shift buffers to make room for new data (each buffer has its own size!)
	for (auto i = 0; i < valbufsize - bufsize; i++) {
		valbuf[i] = valbuf[bufsize + i];
	}

	for (auto i = 0; i < magbufsize - bufsize; i++) {
		magbuf[i] = magbuf[bufsize + i];
	}

	// Add new data to the end of buffers
	for (auto i = 0; i < bufsize; i++) {
		valbuf[valbufsize - bufsize + i] = buf[i];
		magbuf[magbufsize - bufsize + i] = std::abs(buf[i]);
	}

	// Calculate gain for each output sample
	// This implements: gain = np.ravel(np.outer(magbuf, agcshape))[ind].max(1)
	// The outer product creates a matrix where element [i,j] = magbuf[i] * agcshape[j]
	// We use the precomputed index matrix to find the maximum along diagonal stripes
	std::vector<double> gain;
	gain.reserve(bufsize);

	for (const auto& row : ind) {
		double maxval = 0.0;
		for (auto idx : row) {
			// Compute the value from the raveled outer product
			// In a raveled matrix with agcshape.size() columns:
			// element at position idx corresponds to magbuf[row] * agcshape[col]
			size_t outrow = idx / agcshape.size();
			size_t outcol = idx % agcshape.size();
			double val = magbuf[outrow] * agcshape[outcol];
			maxval = std::max(maxval, val);
		}
		gain.push_back(maxval);
	}

	// Apply gain transformation: gain = maxoutnorm * (1 - exp(-gain/beta)) / gain
	// Clamp gain to minimum value to avoid division by zero
	res.reserve(bufsize);
	for (auto i = 0; i < bufsize; i++) {
		double g = std::max(gain[i], 1.0e-8);
		g = maxoutnorm * (1.0 - std::exp(-g / beta)) / g;
		res.push_back(g * valbuf[i]);
	}

	return res;
}
