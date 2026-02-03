
#include <cmath>
#include <algorithm>

#include "station.h"

static std::map<station_message, std::string> msg2text = {
	{ station_message::cq,          "TEST <my>"                 },
	{ station_message::nr,          "<#>"                       },
	{ station_message::tu,          "TU"                        },
	{ station_message::mycall,      "<my>"                      },
	{ station_message::hiscall,     "<my>"                      },
	{ station_message::b4,          "QSO B4"                    },
	{ station_message::qm,          "?"                         },
	{ station_message::nil,         "NIL"                       },
	{ station_message::r_nr,        "R <#>"                     },
	{ station_message::r_nr2,       "R <#> <#>"                 },
	{ station_message::demycall1,   "<my>"                      },
	{ station_message::demycall2,   "<my> <my>"                 },
	{ station_message::demycallnr1, "<my> <#>"                  },
	{ station_message::demycallnr2, "DE <my> <my> <#>"          },
	{ station_message::mycallnr2,   "<my> <my> <#>"             },
	{ station_message::nrqm,        "NR?"                       },
	{ station_message::longcq,      "CQ CQ TEST <my> <my> TEST" },
	{ station_message::qrl,         "QRL?"                      },
	{ station_message::qrl2,        "QRL?   QRL?"               },
	{ station_message::qsy,         "<his> QSY QSY"             },
	{ station_message::agn,         "AGN"                       },
};

station::station(RNG *rng_, Keyer *keyer_, size_t bufsize_, size_t rate_)
{
	rng = rng_;
	keyer = keyer_;
	bufsize = bufsize_;
	rate = rate_;

	set_pitch(500);


	amplitude = 0.7;
	wpm = 30.0;
	rst = 599;
	nr = 1;
	nr_err = false;
	standard_abbrevs = true;

	state = station_state::listening;

	// Pre-reserve msgs capacity to avoid reallocation in audio thread
	msgs.reserve(4);

	sendpos = 0;

	timeout = NEVER;

	buffer.resize(bufsize, 0.0);
	bfo.resize(bufsize, 0.0);
}

void station::set_pitch(double pitch_)
{
	pitch = pitch_;
	dphi = 2.0 * M_PI * pitch / static_cast<double>(rate);
	fbfo = 0.0;
}


const std::vector<double> &station::get_bfo()
{
	// Use incremental phase accumulation to maintain continuity
	// Keep bfo[] values continuous (unwrapped) within the buffer to avoid clicks
	// Use double precision throughout to match Python
	double phase = fbfo;
	const double two_pi = 2.0 * M_PI;

	for (size_t i = 0; i < bufsize; i++) {
		bfo[i] = phase;
		phase += dphi;
	}

	// Wrap fbfo for next buffer to avoid precision loss with large values
	fbfo = std::fmod(phase, two_pi);
	if (fbfo < 0.0) {
		fbfo += two_pi;
	}

	return bfo;
}


const std::vector<double> &station::get_buffer()
{
	std::fill(buffer.begin(), buffer.end(), 0.0);

	size_t max = std::min(sendpos + bufsize, envelope.size());

	for (unsigned long i = 0, j = sendpos; j < max; i++, j++) {
		buffer[i] = envelope[j];
	}

	sendpos += bufsize;

	if (sendpos >= envelope.size()) {
		envelope.clear();
		sendpos = 0;
	}

	return buffer;
}


void station::tick()
{
	if ((state == station_state::sending) && envelope.empty()) {
		msgtext = "";
		state = station_state::listening;
		processEvent(station_event::msgsent);
	} else if (state != station_state::sending) {
		if (timeout != NEVER) {
			timeout -= 1;
			if (timeout < 0) {
				processEvent(station_event::timeout);
			}
		}
	}
}

static bool replace(std::string &str, const std::string &oldval, const std::string &newval)
{
	size_t pos;
	if ((pos = str.find(oldval)) != std::string::npos) {
		str.replace(pos, oldval.length(), newval);
		return true;
	}

	return false;
}

std::string station::nrAsText()
{
	char buffer[16];

	snprintf(buffer, 16, "%d %03d", rst, nr);

	if (nr_err) {
		if (buffer[7] >= '2' && buffer[7] <= '7') {
			if (rng->random() < 0.5)
				snprintf(buffer, 16, "%d %03deeeee%03d", rst, nr - 1, nr);
			else
				snprintf(buffer, 16, "%d %03deeeee%03d", rst, nr + 1, nr);
		} else if (buffer[6] >= '2' && buffer[6] <= '7') {
			if (rng->random() < 0.5)
				snprintf(buffer, 16, "%d %03deeeee%03d", rst, nr - 10, nr);
			else
				snprintf(buffer, 16, "%d %03deeeee%03d", rst, nr + 10, nr);
		}

		nr_err = false;
	}

	std::string s(buffer);

	if (standard_abbrevs) {
		while (replace(s, "9", "N"));
		while (replace(s, "0", "T"));
	} else {
		replace(s, "599", "5NN");
		replace(s, "000", "TTT");
		replace(s, "00", "TT");

		if (rng->random() < 0.4) {
			replace(s, "0", "O");
		} else if (rng->random() < 0.97) {
			replace(s, "0", "T");
		}

		if (rng->random() < 0.97) {
			replace(s, "9", "N");
		}
	}

	return s;
}


void station::sendText(const std::string &text)
{
	std::string str = text;

	while (replace(str, "<#>", nrAsText()));
	while (replace(str, "<my>", mycall));
	while (replace(str, "<his>", hiscall));

	if (msgtext != "")
		msgtext = msgtext + " " + str;
	else
		msgtext = str;

	std::transform(msgtext.begin(), msgtext.end(), msgtext.begin(), [](unsigned char c){ return std::tolower(c); });

	auto s = keyer->Encode(msgtext);
	envelope = keyer->getEnvelope(s, wpm);

	std::transform(envelope.begin(), envelope.end(), envelope.begin(),
		       [this](double e) { return e * this->amplitude; });

	state = station_state::sending;
	timeout = NEVER;
}


void station::sendMsg(station_message msg)
{
	if (envelope.empty()) {
		msgs.clear();
	}

	if (msg == station_message::nomsg) {
		state = station_state::listening;
	} else {
		msgs.push_back(msg);
		sendText(msg2text[msg]);
	}

}
