#include <cmath>
#include <cassert>

#include <algorithm>

#include "audioprocess.h"

MovAvg::MovAvg(size_t bufsize_, size_t navg_)
	: bufsize(bufsize_), navg(navg_), inew(1)
{
	assert(bufsize > navg);
	sums[0].resize(bufsize);
	sums[1].resize(bufsize);
	std::fill(sums[0].begin(), sums[0].end(), std::complex<double>(0.0, 0.0));
	std::fill(sums[1].begin(), sums[1].end(), std::complex<double>(0.0, 0.0));
}

void MovAvg::avg(const std::complex<double> *in, std::complex<double> *out)
{
	// Compute cumulative sum of input buffer
	// cumsum[i] = sum of in[0..i]
	std::complex<double> cumsum(0.0, 0.0);
	for (size_t i = 0; i < bufsize; i++) {
		cumsum += in[i];
		sums[inew][i] = cumsum;
	}

	int iold = 1 - inew;

	// For the first navg outputs, we need data from the previous buffer
	// avg[i] = (sums[inew][i] + sums[iold][bufsize-1] - sums[iold][bufsize-navg+i]) / navg
	for (size_t i = 0; i < navg; i++) {
		out[i] = (sums[inew][i] + sums[iold][bufsize - 1] - sums[iold][bufsize - navg + i])
		         / static_cast<double>(navg);
	}

	// For the remaining outputs, use current cumsum differences
	// avg[i] = (sums[inew][i] - sums[inew][i - navg]) / navg
	for (size_t i = navg; i < bufsize; i++) {
		out[i] = (sums[inew][i] - sums[inew][i - navg]) / static_cast<double>(navg);
	}

	// Toggle buffer
	inew = 1 - inew;
}


void Modulator::setPitch(double pitch)
{
	this->pitch = pitch;

	// Match Python exactly: quantize to integer period
	double period = std::round(static_cast<double>(samplerate) / pitch);
	shift = static_cast<size_t>(bufsize) % static_cast<size_t>(period);
	double dphi = 2.0 * M_PI / period;

	// Pre-compute complex exponential array (matching Python)
	size_t ex_size = bufsize - shift + static_cast<size_t>(period);
	ex.resize(ex_size);

	for (size_t i = 0; i < ex_size; i++) {
		double phase = dphi * static_cast<double>(i);
		if (reverse) {
			// -exp(1j * phase)
			ex[i] = std::complex<double>(-std::cos(phase), -std::sin(phase));
		} else {
			// -exp(-1j * phase)
			ex[i] = std::complex<double>(-std::cos(phase), std::sin(phase));
		}
	}
}

void Modulator::modulate(const std::complex<double> *in, double *out)
{
	// Multiply and take imaginary part (matching Python)
	for (size_t i = 0; i < bufsize; i++) {
		out[i] = std::imag(ex[i] * in[i]);
	}

	// Roll the array by -shift (equivalent to np.roll(ex, -shift))
	if (shift > 0 && shift < ex.size()) {
		std::rotate(ex.begin(), ex.begin() + shift, ex.end());
	}
}

Agc::Agc(size_t bufsize, double maxout, double maxoutnorm, double noiseindb, double noiseoutdb, size_t attacksamples, size_t holdsamples):
	bufsize(bufsize), maxoutnorm(maxoutnorm)
{
	//size_t agcbufsize;

	noisein = std::pow(10.0, 0.05 * noiseindb);
	noiseout = std::min(std::pow(10.0, 0.05 * noiseoutdb), 0.25 * maxout);

	beta = noisein / std::log(maxout / (maxout - noiseout));

	agcmid =  attacksamples + holdsamples;
	agcbufsize = 2 * agcmid + 1;

	//agcshape.resize(agcbufsize);
	//std::fill(agcshape.begin(), agcshape.end(), 1.0);
	agcshape = (double *) malloc(agcbufsize * sizeof(double));
	for (auto i = 0; i < agcbufsize; i++) {
		agcshape[i] = 1.0;
	}

	for (auto i = 0; i < attacksamples; i++) {
		agcshape[i] = 0.5 - 0.5 * std::cos( M_PI * (i + 1) / (attacksamples + 1));
		agcshape[agcbufsize - i - 1] = agcshape[i];
	}

	magbufsize = agcbufsize + bufsize - 1;
	//magbuf.resize(magbufsize);
	//std::fill(magbuf.begin(), magbuf.end(), 0.0);

	magbuf = (double *) malloc(magbufsize * sizeof(double));
	for (auto i = 0; i < magbufsize; i++) {
		magbuf[i] = 0.0;
	}

	valbufsize = agcmid + bufsize;
	//valbuf.resize(valbufsize);
	//std::fill(valbuf.begin(), valbuf.end(), 0.0);

	valbuf = (double *) malloc(valbufsize * sizeof(double));
	for (auto i = 0; i < valbufsize; i++) {
		valbuf[i] = 0.0;
	}


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

	//gain.resize(bufsize);
	gain = (double *) malloc(bufsize * sizeof(double));
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
			//size_t outrow = idx / agcshape.size();
			//size_t outcol = idx % agcshape.size();

			size_t outrow = idx / agcbufsize;
			size_t outcol = idx % agcbufsize;

			double val = magbuf[outrow] * agcshape[outcol];
			maxval = std::max(maxval, val);
		}
		gain[k++] = maxval;
	}

	// Apply gain transformation: gain = maxoutnorm * (1 - exp(-gain/beta)) / gain
	for (size_t i = 0; i < bufsize; i++) {
		double g = std::max(gain[i], 1.0e-8);
		g = maxoutnorm * (1.0 - std::exp(-g / beta)) / g;
		// Use valbuf[i], not valbuf[agcmid + i] - Python uses valbuf[:bufsize]
		out[i] = g * valbuf[i];

		// Clamp output to [-1.0, 1.0] to prevent clipping
		// This handles attack overshoot and weak signal high-gain cases
		// no, claming does not HANDLE overshoot. It should not overshoot so badly
		//out[i] = std::clamp(out[i], -1.0, 1.0);
	}

	//gain = np.ravel(np.outer(self._magbuf,self._agcshape))[self._ind].max(1)
	//gain = np.maximum(gain,1.0e-8)
	//gain = self._maxoutnorm*(1.0-np.exp(-gain/self._beta))/gain
        //return gain*self._valbuf[:self._bufsize]


}
