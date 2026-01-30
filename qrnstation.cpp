#include "qrnstation.h"
#include <cmath>

QrnStation::QrnStation(RNG *rng, size_t bufsize, size_t rate)
	: station(rng, nullptr, bufsize, rate)
{
	// Generate random duration (1-2 seconds worth of buffers)
	int num_buffers = static_cast<int>(std::round(rng->uniform() * rate / bufsize)) + 1;
	size_t nenv = num_buffers * bufsize;

	// Random amplitude: 10^5 to 10^7 range (very strong noise bursts)
	double amp = 1.0e5 * std::pow(10.0, 2.0 * rng->uniform());

	// Generate envelope with random values
	envelope.clear();
	envelope.reserve(nenv);

	for (size_t i = 0; i < nenv; i++) {
		// Random value from -0.5 to 0.5, scaled by amplitude
		envelope.push_back(amp * (rng->uniform() - 0.5));
	}

	// Zero out 99% of samples to create sparse "crash" effect
	// This gives atmospheric noise its characteristic sound
	for (size_t i = 0; i < nenv; i++) {
		if (rng->uniform() < 0.99) {
			envelope[i] = 0.0;
		}
	}

	// Start transmitting immediately
	this->state = station_state::sending;
}

void QrnStation::processEvent(station_event evt)
{
	if (evt == station_event::msgsent) {
		// Noise burst finished, delete this station
		this->state = station_state::deleteme;
	}
}
