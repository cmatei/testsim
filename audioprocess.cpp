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

void MovAvg::avg(const std::complex<double> *in, std::complex<double> *out)
{
	std::complex<double> sum(0.0, 0.0);
	size_t k = 0;

	// Copy new data into prev buffer at the end
	for (size_t i = navg - 1; i < bufsize + navg - 1; i++) {
		prev[i] = in[i - navg + 1];
	}

	// Calculate moving average
	for (size_t i = 0; i < bufsize + navg; i++) {
		sum += prev[i];
		if (i >= navg) {
			out[k++] = sum / static_cast<double>(navg);
			sum -= prev[i - navg];
		}
	}

	// Save the tail for next iteration
	for (size_t i = bufsize - navg; i < bufsize; i++) {
		prev[i - bufsize + navg] = in[i];
	}
}


void Modulator::setPitch(double pitch)
{
	this->pitch = pitch;
	// Compute exact phase increment (no quantization!)
	dphi = 2.0 * M_PI * pitch / samplerate;
}


void Modulator::modulate(const std::complex<double> *in, double *out)
{
	// Compute complex exponential on-the-fly with exact phase
	double sign = reverse ? 1.0 : -1.0;
	double p = phase;

	for (size_t i = 0; i < bufsize; i++) {
		// Direct phase calculation: exp(-j*p) = cos(p) - j*sin(p)
		std::complex<double> ex(std::cos(p), sign * std::sin(p));
		out[i] = std::imag(ex * in[i]);
		p += dphi;
	}

	// Wrap phase to prevent precision loss (same as station::get_bfo)
	phase = std::fmod(p, 2.0 * M_PI);
	if (phase < 0.0) {
		phase += 2.0 * M_PI;
	}
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

	gain.resize(bufsize);
}


void Agc::process(const double *in, double *out)
{
	// Shift buffers to make room for new data (each buffer has its own size!)
	for (size_t i = 0; i < valbufsize - bufsize; i++) {
		valbuf[i] = valbuf[bufsize + i];
	}

	for (size_t i = 0; i < magbufsize - bufsize; i++) {
		magbuf[i] = magbuf[bufsize + i];
	}

	// Add new data to the end of buffers
	for (size_t i = 0; i < bufsize; i++) {
		valbuf[valbufsize - bufsize + i] = in[i];
		magbuf[magbufsize - bufsize + i] = std::abs(in[i]);
	}

	// Calculate gain for each output sample
	size_t k = 0;
	for (const auto& row : ind) {
		double maxval = 0.0;
		for (auto idx : row) {
			size_t outrow = idx / agcshape.size();
			size_t outcol = idx % agcshape.size();
			double val = magbuf[outrow] * agcshape[outcol];
			maxval = std::max(maxval, val);
		}
		gain[k++] = maxval;
	}

	// Apply gain transformation: gain = maxoutnorm * (1 - exp(-gain/beta)) / gain
	for (size_t i = 0; i < bufsize; i++) {
		double g = std::max(gain[i], 1.0e-8);
		g = maxoutnorm * (1.0 - std::exp(-g / beta)) / g;
		//out[i] = g * valbuf[agcmid + i];  // Fixed: apply gain to current sample, not old data
		out[i] = g * valbuf[i];
	}
}
