#include "mystation.h"
#include "contest.h"
#include <algorithm>
#include <cmath>

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

	// Clear pieces and accumulated text
	pieces.clear();
	full_text.clear();

	// Return to listening state
	this->state = station_state::listening;

	// Trigger the msgsent event
	processEvent(station_event::msgsent);
}

void MyStation::detectAndSetMessages()
{
	// Guess message type from full accumulated text, so dxstations
	// can processEvent in Contest::onMeFinishedSending()
	msgs.clear();

	const std::string &msg = full_text;

	// CQ detection
	if (msg.find("CQ ") == 0 || msg.find("TEST ") == 0) {
		msgs.push_back(station_message::cq);
		return;
	}

	// NR at start: "5NN 001"
	if (msg.find("5NN ") == 0) {
		msgs.push_back(station_message::nr);
		return;
	}

	// Call + NR: "W1AW 5NN 001"
	size_t nr_pos = msg.find(" 5NN ");
	if (nr_pos != std::string::npos) {
		hiscall = msg.substr(0, nr_pos);
		msgs.push_back(station_message::hiscall);
		msgs.push_back(station_message::nr);
		return;
	}

	// TU variants
	if (msg == "TU") {
		msgs.push_back(station_message::tu);
		return;
	}
	if (msg.find("TU ") == 0) {
		msgs.push_back(station_message::tu);
		return;
	}
	size_t tu_pos = msg.find(" TU");
	if (tu_pos != std::string::npos) {
		hiscall = msg.substr(0, tu_pos);
		msgs.push_back(station_message::hiscall);
		msgs.push_back(station_message::tu);
		return;
	}

	// Question marks
	if (msg.find("?") == 0) {
		msgs.push_back(station_message::qm);
		return;
	}
	if (msg.find("NR?") == 0) {
		msgs.push_back(station_message::nrqm);
		return;
	}
	if (msg.find("AGN") != std::string::npos) {
		msgs.push_back(station_message::agn);
		return;
	}

	// Default: partial call or unknown message
	if (!msg.empty()) {
		hiscall = msg;
		msgs.push_back(station_message::hiscall);
	} else {
		msgs.push_back(station_message::nomsg);
	}
}

void MyStation::sendText(const std::string &msg)
{
	size_t pos;

	bool is_continuation = (this->state == station_state::sending);

	// Accumulate full text while sending
	if (is_continuation) {
		// Continuation: append with space separator
		full_text += " " + msg;
	} else {
		// First piece: start fresh accumulation
		full_text = msg;
	}

	// Split message on '<his>' placeholders
	std::string remaining = msg;
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

	// Detect message type from accumulated text (do this in main thread, not audio thread)
	detectAndSetMessages();

	// If not currently sending, start sending
	if (!is_continuation) {
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

	bool continuation = (state == station_state::sending);

	std::string text = (pieces[0] != "@") ? pieces[0] : hiscall;

	// For word space, strip leading space (we insert the gap ourselves)
	bool word_space = continuation && !text.empty() && text[0] == ' ';
	if (word_space) {
		text = text.substr(1);
	}

	// Reset sendpos when starting a new piece
	sendpos = 0;

	station::sendText(text);

	// Prepend inter-piece silence for continuation pieces
	// Previous piece ends with ~2 dits (samples-nr + samples-nr from keyer)
	if (continuation) {
		size_t dit = static_cast<size_t>(std::round(1.2 * rate / wpm));
		size_t gap = word_space ? 5 * dit : 1 * dit;
		envelope.insert(envelope.begin(), gap, 0.0f);
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
