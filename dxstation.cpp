#include "dxstation.h"
#include <cmath>
#include <algorithm>

DxStation::DxStation(RNG *rng, Keyer *keyer, CallList *callList, station *cqstn_,
                     int minutes, bool lids, double lidNrProb, double lidRstProb,
                     bool qsb, double flutterProb, double rptProb, double fast,
                     double slow, bool isSingle, int norepeats, size_t bufsize, size_t rate)
	: station(rng, keyer, bufsize, rate), cqstn(cqstn_), qsb_effect(nullptr),
	  called(false)
{
	// Set up station identity
	this->hiscall = cqstn->mycall;
	this->mycall = callList->pick();

	// Create the operator state machine
	oper = new DxOperator(rng, minutes, cqstn, this->mycall,
	                      rng->integers(1, 4),  // skills 1-3
	                      static_cast<double>(rate) / bufsize,  // s2bfac
	                      lids, rptProb, cqstn->wpm, fast, slow, isSingle, norepeats,
	                      OperatorState::NeedPrevEnd);

	// Set up exchange number (possibly with error for lids)
	bool nrWithError = lids && (rng->uniform() < lidNrProb);
	this->nr = oper->getNr();

	// Set up WPM
	this->wpm = oper->getWpm();

	// Set up RST (possibly with error for lids)
	if (lids && rng->uniform() < lidRstProb) {
		rst_to_send = 559 + 10 * rng->integers(0, 4);
	} else {
		rst_to_send = 599;
	}
	this->rst = rst_to_send;

	// Set up QSB fading effect
	if (qsb) {
		if (rng->uniform() < flutterProb) {
			// Fast flutter
			qsb_effect = new QSB(rng, 3.0 + 30.0 * rng->uniform(), bufsize, rate);
		} else {
			// Slow fading
			qsb_effect = new QSB(rng, 0.1 + 0.5 * rng->uniform(), bufsize, rate);
		}
	}

	// Set amplitude with random variation
	this->amplitude = 9000.0 + 18000.0 * (1.0 + std::sin(M_PI * (rng->uniform() - 0.5)));

	// Set pitch offset from center (0 = on frequency, heard at modulator pitch)
	double pitch_offset = std::clamp(rng->normal(0.0, 100.0), -300.0, 300.0);
	this->set_pitch(pitch_offset);

	// Start in Copying state (listening to CQ station)
	this->state = station_state::copying;
}

DxStation::~DxStation()
{
	delete oper;
	if (qsb_effect) {
		delete qsb_effect;
	}
}

void DxStation::processEvent(station_event evt)
{
	// If operator is done, don't process any more events
	if (oper->state == OperatorState::Done) {
		return;
	}

	switch (evt) {
	case station_event::msgsent:
		// Message transmission completed
		if (cqstn->state == station_state::sending) {
			// CQ station is still sending, wait indefinitely
			this->timeout = NEVER;
		} else {
			// Set timeout for reply from CQ station
			this->timeout = oper->getReplyTimeout();
		}
		break;

	case station_event::timeout:
		if (this->state == station_state::listening) {
			// Timeout while listening - tell operator no message received
			oper->msgReceived({station_message::nomsg});
			if (oper->state == OperatorState::Failed) {
				this->state = station_state::deleteme;
				return;
			}
			this->state = station_state::preparingtosend;
		}

		if (this->state == station_state::preparingtosend) {
			// Time to send - get reply from operator and transmit it
			for (int i = 0; i < oper->repeatCnt; i++) {
				station_message reply = oper->getReply();
				called |= (reply != station_message::nomsg);
				sendMsg(reply);
			}
		}
		break;

	case station_event::mefinished:
		// CQ station finished sending
		if (this->state != station_state::sending) {
			if (this->state == station_state::copying) {
				// Was copying - pass messages to operator
				oper->msgReceived(cqstn->msgs);
			} else if (this->state == station_state::listening ||
			           this->state == station_state::preparingtosend) {
				// Was listening/preparing - check what CQ station sent
				bool hasCQ = std::find(cqstn->msgs.begin(), cqstn->msgs.end(),
				                       station_message::cq) != cqstn->msgs.end();
				bool hasTU = std::find(cqstn->msgs.begin(), cqstn->msgs.end(),
				                       station_message::tu) != cqstn->msgs.end();
				bool hasNil = std::find(cqstn->msgs.begin(), cqstn->msgs.end(),
				                        station_message::nil) != cqstn->msgs.end();

				if (hasCQ || hasTU || hasNil) {
					oper->msgReceived(cqstn->msgs);
				} else {
					// Garbage message
					oper->msgReceived({station_message::nomsg});
				}
			}

			if (oper->state == OperatorState::Failed) {
				this->state = station_state::deleteme;
				return;
			}

			// Set delay before sending
			this->timeout = oper->getSendDelay();
			this->state = station_state::preparingtosend;
		}
		break;

	case station_event::mestarted:
		// CQ station started sending
		if (this->state != station_state::sending) {
			this->state = station_state::copying;
		}
		this->timeout = NEVER;
		break;
	}
}

const std::vector<float> &DxStation::get_buffer()
{
	// Get base buffer from station class
	const std::vector<float> &buf = station::get_buffer();

	// Apply QSB fading if enabled
	if (qsb_effect) {
		qsb_effect->applyTo(const_cast<std::vector<float>&>(buf));
	}

	return buf;
}

DxStation::QsoData DxStation::dataToLastQso()
{
	this->state = station_state::deleteme;
	return {this->mycall, rst_to_send, this->nr, static_cast<int> (std::round(this->wpm))};
}
