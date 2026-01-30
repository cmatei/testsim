#include "dxoper.h"
#include <algorithm>
#include <cmath>

DxOperator::DxOperator(RNG *rng_, int minutes_, station *cqstn_,
                       const std::string &call, int skills_, double s2bfac_,
                       bool lids_, double rptProb_, int wpm_, double fast_,
                       double slow_, bool isSingle_, OperatorState state_)
	: rng(rng_), minutes(minutes_), cqstn(cqstn_), myCall(call),
	  skills(skills_), isSingle(isSingle_), lids(lids_), wpm(wpm_),
	  slow(slow_), fast(fast_), rptProb(rptProb_), s2bfac(s2bfac_),
	  state(state_), patience(0), repeatCnt(1)
{
}

int DxOperator::getSendDelay()
{
	if (state == OperatorState::NeedPrevEnd) {
		return NEVER;
	} else {
		return static_cast<int>(s2bfac * (0.1 + 0.5 * rng->uniform()));
	}
}

int DxOperator::getWpm()
{
	double speed = wpm * (slow + (fast - slow) * rng->uniform());
	return static_cast<int>(std::round(speed));
}

int DxOperator::getNr()
{
	return static_cast<int>(std::round(1 + rng->uniform() * minutes * skills));
}

int DxOperator::getReplyTimeout()
{
	double b = s2bfac * (6 - skills);
	double timeout = rng->normal(b, 0.25 * b);
	return static_cast<int>(std::clamp(timeout, 0.5 * b, 1.5 * b));
}

void DxOperator::decPatience()
{
	if (state != OperatorState::Done) {
		patience--;
		if (patience < 1) {
			state = OperatorState::Failed;
		}
	}
}

void DxOperator::setState(OperatorState newState)
{
	state = newState;

	if (state == OperatorState::NeedQso) {
		// Rayleigh distribution with scale parameter 3.191538
		patience = static_cast<int>(std::round(rng->rayleigh(3.191538)));
	} else {
		patience = FULL_PATIENCE;
	}

	if (state == OperatorState::NeedQso && !isSingle && rng->uniform() < 0.1) {
		repeatCnt = 2;
	} else {
		repeatCnt = 1;
	}
}

MyCallCorrect DxOperator::isMyCall()
{
	// Calculate edit distance from cq station's reply to my call
	// This implements the Levenshtein distance algorithm with wildcards ('?')

	const std::string &c0 = myCall;
	const std::string &c = cqstn->hiscall;

	size_t len_c = c.length();
	size_t len_c0 = c0.length();

	// Create distance matrix
	std::vector<std::vector<int>> m(len_c + 1, std::vector<int>(len_c0 + 1, 0));

	// Initialize first column
	for (size_t i = 0; i <= len_c; i++) {
		m[i][0] = i;
	}

	// Fill the matrix
	for (size_t x = 1; x <= len_c; x++) {
		if (c[x-1] != '?') {
			for (size_t y = 1; y <= len_c0; y++) {
				int d = m[x-1][y-1];
				if (c[x-1] != c0[y-1]) d++;
				m[x][y] = std::min({m[x][y-1] + 1, m[x-1][y] + 1, d});
			}
		} else {
			// Wildcard character - no penalty
			for (size_t y = 1; y <= len_c0; y++) {
				m[x][y] = std::min({m[x][y-1], m[x-1][y], m[x-1][y-1]});
			}
		}
	}

	int distance = m[len_c][len_c0];
	MyCallCorrect res;

	if (distance == 0) {
		res = MyCallCorrect::Yes;
	} else if (distance == 1) {
		res = MyCallCorrect::Almost;
	} else {
		res = MyCallCorrect::No;
	}

	// Apply various corrections based on operator skill
	if (!lids && len_c == 2 && res == MyCallCorrect::Almost) {
		res = MyCallCorrect::No;
	}

	if (res == MyCallCorrect::Yes && (len_c != len_c0 || c.find('?') != std::string::npos)) {
		res = MyCallCorrect::Almost;
	}

	// Count non-wildcard characters
	size_t nonWildcardCount = 0;
	for (char ch : c) {
		if (ch != '?') nonWildcardCount++;
	}
	if (nonWildcardCount < 2) {
		res = MyCallCorrect::No;
	}

	// Lids make mistakes
	if (lids && len_c > 3) {
		if (res == MyCallCorrect::Yes && rng->uniform() < 0.01) {
			res = MyCallCorrect::Almost;
		} else if (res == MyCallCorrect::Almost && rng->uniform() < 0.04) {
			res = MyCallCorrect::Yes;
		}
	}

	return res;
}

void DxOperator::msgReceived(const std::vector<station_message> &msgs)
{
	auto contains = [&msgs](station_message msg) {
		return std::find(msgs.begin(), msgs.end(), msg) != msgs.end();
	};

	if (contains(station_message::cq)) {
		if (state == OperatorState::NeedPrevEnd) {
			setState(OperatorState::NeedQso);
		} else if (state == OperatorState::NeedQso) {
			decPatience();
		} else if (state == OperatorState::NeedNr ||
		           state == OperatorState::NeedCall ||
		           state == OperatorState::NeedCallNr) {
			state = OperatorState::Failed;
		} else if (state == OperatorState::NeedEnd) {
			state = OperatorState::Done;
		}
		return;
	}

	if (contains(station_message::nil)) {
		if (state == OperatorState::NeedPrevEnd) {
			setState(OperatorState::NeedQso);
		} else if (state == OperatorState::NeedQso) {
			decPatience();
		} else if (state == OperatorState::NeedNr ||
		           state == OperatorState::NeedCall ||
		           state == OperatorState::NeedCallNr ||
		           state == OperatorState::NeedEnd) {
			state = OperatorState::Failed;
		}
		return;
	}

	if (contains(station_message::hiscall)) {
		MyCallCorrect isme = isMyCall();
		if (isme == MyCallCorrect::Yes) {
			if (state == OperatorState::NeedPrevEnd ||
			    state == OperatorState::NeedQso ||
			    state == OperatorState::NeedCallNr) {
				setState(OperatorState::NeedNr);
			} else if (state == OperatorState::NeedCall) {
				setState(OperatorState::NeedEnd);
			}
		} else if (isme == MyCallCorrect::Almost) {
			if (state == OperatorState::NeedPrevEnd ||
			    state == OperatorState::NeedQso ||
			    state == OperatorState::NeedNr) {
				setState(OperatorState::NeedCallNr);
			} else if (state == OperatorState::NeedEnd) {
				setState(OperatorState::NeedCall);
			}
		} else { // No
			if (state == OperatorState::NeedQso) {
				state = OperatorState::NeedPrevEnd;
			} else if (state == OperatorState::NeedNr ||
			           state == OperatorState::NeedCall ||
			           state == OperatorState::NeedCallNr) {
				state = OperatorState::Failed;
			} else if (state == OperatorState::NeedEnd) {
				state = OperatorState::Done;
			}
		}
	}

	if (contains(station_message::b4)) {
		if (state == OperatorState::NeedPrevEnd ||
		    state == OperatorState::NeedQso) {
			setState(OperatorState::NeedQso);
		} else if (state == OperatorState::NeedNr ||
		           state == OperatorState::NeedEnd) {
			state = OperatorState::Failed;
		}
	}

	if (contains(station_message::nr)) {
		if (state == OperatorState::NeedQso) {
			state = OperatorState::NeedPrevEnd;
		} else if (state == OperatorState::NeedNr) {
			if (rng->uniform() >= rptProb) {
				setState(OperatorState::NeedEnd);
			}
		} else if (state == OperatorState::NeedCallNr) {
			if (rng->uniform() >= rptProb) {
				setState(OperatorState::NeedCall);
			}
		}
	}

	if (contains(station_message::tu)) {
		if (state == OperatorState::NeedPrevEnd) {
			setState(OperatorState::NeedQso);
		} else if (state == OperatorState::NeedEnd) {
			state = OperatorState::Done;
		}
	}

	// NeedQso timeout with no reply: retry the call (patience permitting)
	if (state == OperatorState::NeedQso && msgs.size() == 1 && contains(station_message::nomsg)) {
		decPatience();
		return;
	}

	// Non-lids give up on garbage
	if (!lids && msgs.size() == 1 && contains(station_message::nomsg)) {
		state = OperatorState::NeedPrevEnd;
	}

	if (state != OperatorState::NeedPrevEnd) {
		decPatience();
	}
}

station_message DxOperator::getReply()
{
	station_message res = station_message::nomsg;

	switch (state) {
	case OperatorState::NeedPrevEnd:
	case OperatorState::Done:
	case OperatorState::Failed:
		res = station_message::nomsg;
		break;

	case OperatorState::NeedQso:
		res = station_message::mycall;
		break;

	case OperatorState::NeedNr:
		if (patience == (FULL_PATIENCE - 1) || rng->uniform() < 0.3) {
			res = station_message::nrqm;
		} else {
			res = station_message::agn;
		}
		break;

	case OperatorState::NeedCall:
		{
			double r1 = rng->uniform();
			if (r1 < 0.5) {
				res = station_message::demycallnr1;
			} else if (r1 < 0.625) {
				res = station_message::demycallnr2;
			} else {
				res = station_message::mycallnr2;
			}
		}
		break;

	case OperatorState::NeedCallNr:
		if (rng->uniform() < 0.5) {
			res = station_message::demycall1;
		} else {
			res = station_message::demycall2;
		}
		break;

	case OperatorState::NeedEnd:
		if (patience == (FULL_PATIENCE - 1) || rng->uniform() < 0.9) {
			res = station_message::r_nr;
		} else {
			res = station_message::r_nr2;
		}
		break;
	}

	return res;
}
