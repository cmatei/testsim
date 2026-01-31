#include "qrmstation.h"
#include <cmath>

// Static message list for QRM interference
const std::vector<station_message> QrmStation::qrm_messages = {
	station_message::qrl,
	station_message::qrl2,
	station_message::qrl2,
	station_message::longcq,
	station_message::longcq,
	station_message::longcq,
	station_message::qsy
};

QrmStation::QrmStation(RNG *rng, Keyer *keyer, CallList *callList,
                       const std::string &hisCall,
                       size_t bufsize, size_t rate)
	: station(rng, keyer, bufsize, rate)
{
	// Calculate timeout bounds (2 and 6 seconds worth of buffers)
	b2 = static_cast<int>(std::round(2.0 * rate / bufsize));
	b6 = static_cast<int>(std::round(6.0 * rate / bufsize));

	// Set patience (number of transmissions before giving up)
	patience = rng->integers(1, 6);  // 1-5

	// Set station identities
	this->hiscall = hisCall;
	this->mycall = callList->pick();

	// Random amplitude (5000-30000 range)
	this->amplitude = 5000.0 + 25000.0 * rng->uniform();

	// Random pitch offset from center frequency (-300 to +300 Hz)
	int pitch_offset = rng->integers(-300, 301);
	this->set_pitch(pitch_offset);

	// Random WPM (30-50)
	this->wpm = rng->integers(30, 51);

	// Send a random QRM message to start
	int msg_idx = rng->integers(0, static_cast<int>(qrm_messages.size()));
	sendMsg(qrm_messages[msg_idx]);
}

void QrmStation::processEvent(station_event evt)
{
	if (evt == station_event::msgsent) {
		// Message transmission completed
		patience--;

		if (patience > 0) {
			// Set timeout for next transmission (2-6 seconds)
			this->timeout = rng->integers(b2, b6 + 1);  // +1 for inclusive upper bound
		} else {
			// Ran out of patience, delete this QRM station
			this->state = station_state::deleteme;
		}
	} else if (evt == station_event::timeout) {
		// Timeout expired, send another long CQ
		sendMsg(station_message::longcq);
	}
}
