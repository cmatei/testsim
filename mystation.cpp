#include "mystation.h"
#include "contest.h"
#include <algorithm>

MyStation::MyStation(RNG *rng, Keyer *keyer, Contest *contest_,
                     const std::string &myCall, float pitch_, int wpm_,
                     size_t bufsize, size_t rate)
	: station(rng, keyer, bufsize, rate), contest(contest_)
{
	this->mycall = myCall;
	this->nr = 1;
	this->rst = 599;
	this->set_pitch(pitch_);
	this->wpm = wpm_;
	this->amplitude = 1.0;
}

void MyStation::processEvent(station_event evt)
{
	if (evt == station_event::msgsent) {
		// Notify contest that we finished sending
		if (contest) {
			contest->onMeFinishedSending();
		}
	}
}

void MyStation::abortSend()
{
	// Clear the envelope and reset send position
	envelope.clear();
	sendpos = 0;

	// Mark as garbage message
	msgs.clear();
	msgs.push_back(station_message::nomsg);
	msgtext.clear();

	// Clear pieces
	pieces.clear();

	// Return to listening state
	this->state = station_state::listening;

	// Trigger the msgsent event
	processEvent(station_event::msgsent);
}

void MyStation::detectMessage(const std::string &msg)
{
	// Guess message type from full text, so dxstations
	// can processEvent in Contest::onMeFinishedSending()
	msgs.clear();
	if ((msg.rfind("CQ ", 0) != std::string::npos) ||
	    (msg.rfind("TEST ", 0) != std::string::npos)) {
		msgs.push_back(station_message::cq);
	}

	if (msg.find("5NN ") != std::string::npos) {
		hiscall = msg.substr(0, msg.find(" "));
		msgs.push_back(station_message::hiscall);
		msgs.push_back(station_message::nr);
	}

	if (msg == "TU" || (msg.rfind("TU ", 0) != std::string::npos)) {
		msgs.push_back(station_message::tu);
	}

	if (msg.rfind(" TU") != std::string::npos) {
		hiscall = msg.substr(0, msg.find(" "));
		msgs.push_back(station_message::hiscall);
		msgs.push_back(station_message::tu);
	}

	if (msg.rfind("?", 0) != std::string::npos) {
		msgs.push_back(station_message::qm);
	}

	if (msg.rfind("NR?", 0) != std::string::npos) {
		msgs.push_back(station_message::nrqm);
	}

	if (msg.rfind("AGN", 0) != std::string::npos) {
		msgs.push_back(station_message::agn);
	}
}

void MyStation::sendText(const std::string &msg)
{
	size_t pos;

	// Detect message type if not already set by an explicit
	// detectMessage() call (e.g. cwdaemon with inline speed changes)
	if (this->state != station_state::sending && msgs.empty()) {
		detectMessage(msg);
	}

	std::string remaining = msg;

	// Split message on '<his>' placeholders
	pos = remaining.find("<his>");
	while (pos != std::string::npos) {
		// Add text before '<his>' if any
		if (pos != 0) {
			pieces.push_back(remaining.substr(0, pos));
		}

		// Add '@' marker for call placeholder
		pieces.push_back("@");

		// Continue with rest of string
		remaining = remaining.substr(pos + 5);
		pos = remaining.find("<his>");
	}

	// Add any remaining text
	if (!remaining.empty()) {
		pieces.push_back(remaining);
	}

	// If not currently sending, start sending
	if (this->state != station_state::sending) {
		sendNextPiece();
		if (contest) {
			contest->onMeStartedSending();
		}
	}
}

void MyStation::sendNextPiece()
{
	msgtext.clear();

	if (pieces.empty()) {
		return;
	}

	if (pieces[0] != "@") {
		// Send the text piece
		station::sendText(pieces[0]);
	} else {
		// Send the call sign
		station::sendText(hiscall);
	}
}

const std::vector<float> &MyStation::get_buffer()
{
	// Get buffer from parent class
	const std::vector<float> &buf = station::get_buffer();

	// If envelope is done (empty), move to next piece
	if (envelope.empty()) {
		if (!pieces.empty()) {
			pieces.erase(pieces.begin());  // Pop first element

			if (!pieces.empty()) {
				sendNextPiece();
			}
		}
	}

	return buf;
}

bool MyStation::updateCallInMessage(const std::string &call)
{
	// Dynamic call sign update while sending
	// This allows updating the call as the user types

	if (call.empty()) {
		return false;
	}

	bool result = false;

	// Check if we're currently sending a call placeholder
	if (!pieces.empty()) {
		result = (pieces[0] == "@");
	}

	if (result) {
		// We're sending a call placeholder
		// Re-encode the new call and check if it matches what we've sent so far
		std::string lower_call = call;
		std::transform(lower_call.begin(), lower_call.end(),
		               lower_call.begin(), ::tolower);

		// Encode the new call
		std::string encoded = keyer->Encode(lower_call);
		std::vector<float> new_envelope = keyer->getEnvelope(encoded, wpm);

		// Scale by amplitude
		for (auto &val : new_envelope) {
			val *= amplitude;
		}

		// Check if new envelope is at least as long as what we've sent
		result = new_envelope.size() >= sendpos;

		if (result) {
			// Check if what we've sent so far matches
			result = true;
			for (size_t i = 0; i < sendpos && i < new_envelope.size() && i < envelope.size(); i++) {
				if (std::abs(envelope[i] - new_envelope[i]) > 1e-6) {
					result = false;
					break;
				}
			}
		}

		if (result) {
			// Update the envelope with the new call
			envelope = new_envelope;
			this->hiscall = call;
		}
	} else {
		// Not currently sending a call, but check if there's a call placeholder coming
		std::vector<std::string> tmp = pieces;
		if (!tmp.empty()) {
			tmp.erase(tmp.begin());  // Remove first element

			// Check if there's an '@' in the remaining pieces
			if (std::find(tmp.begin(), tmp.end(), "@") != tmp.end()) {
				result = true;
				this->hiscall = call;
			}
		}
	}

	return result;
}
