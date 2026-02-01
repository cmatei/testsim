#include "dxoper.h"
#include <algorithm>
#include <cmath>

DxOperator::DxOperator(RNG *rng_, int minutes_, station *cqstn_,
                       const std::string &call, int skills_, double s2bfac_,
                       bool lids_, double rptProb_, int wpm_, double fast_,
                       double slow_, bool isSingle_, int norepeats_, int longnr_, OperatorState state_)
	: rng(rng_), minutes(minutes_), cqstn(cqstn_), myCall(call),
	  skills(skills_), isSingle(isSingle_), lids(lids_), wpm(wpm_),
	  slow(slow_), fast(fast_), rptProb(rptProb_), s2bfac(s2bfac_),
	  norepeats(norepeats_), longnr(longnr_), state(state_), patience(0), repeatCnt(1)
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
	if (longnr) {
		// Generate random 4-digit numbers for training (1-9999)
		return 1 + rng->integers(0, 9999);
	} else {
		// Time-based serials that grow over contest duration
		return static_cast<int>(std::round(1 + rng->uniform() * minutes * skills));
	}
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
		// Set patience to at least half of max, plus some randomness
		// Range: [FULL_PATIENCE/2, FULL_PATIENCE] = [4, 8]
		patience = FULL_PATIENCE / 2 + rng->integers(0, FULL_PATIENCE / 2 + 1);
	} else {
		patience = FULL_PATIENCE;
	}

	if (state == OperatorState::NeedQso && !isSingle && !norepeats && rng->uniform() < 0.1) {
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

	// Check for partial call (substring match)
	// If sent call is shorter and appears anywhere in my call, treat as "Almost"
	if (len_c < len_c0 && len_c >= 2) {
		// Try to find c as a substring in c0
		for (size_t start = 0; start <= len_c0 - len_c; start++) {
			bool matches = true;
			for (size_t i = 0; i < len_c; i++) {
				if (c[i] != '?' && c[i] != c0[start + i]) {
					matches = false;
					break;
				}
			}
			if (matches) {
				// Partial call found as substring - return Almost
				return MyCallCorrect::Almost;
			}
		}
	}

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
			setState(OperatorState::Failed);
		} else if (state == OperatorState::NeedEnd) {
			setState(OperatorState::Done);
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
			setState(OperatorState::Failed);
		}
		return;
	}

	if (contains(station_message::hiscall)) {
		MyCallCorrect isme = isMyCall();
		if (isme == MyCallCorrect::Yes) {
			// Full match - reset patience to full
			patience = FULL_PATIENCE;
			if (state == OperatorState::NeedPrevEnd ||
			    state == OperatorState::NeedQso ||
			    state == OperatorState::NeedCallNr) {
				setState(OperatorState::NeedNr);
			} else if (state == OperatorState::NeedCall) {
  				setState(OperatorState::NeedEnd);
			}
		} else if (isme == MyCallCorrect::Almost) {
			// Partial match - reset patience
			patience = FULL_PATIENCE / 2;
			if (state == OperatorState::NeedPrevEnd ||
			    state == OperatorState::NeedQso ||
			    state == OperatorState::NeedNr) {
				setState(OperatorState::NeedCallNr);
			} else if (state == OperatorState::NeedEnd) {
				setState(OperatorState::NeedCall);
			}
		} else {
			// No - call is not for me
			// When user calls someone else, go quiet and wait for QSO to finish
			if (state == OperatorState::NeedQso ||
			    state == OperatorState::NeedNr ||
			    state == OperatorState::NeedCall ||
			    state == OperatorState::NeedCallNr) {
				setState(OperatorState::NeedPrevEnd);
			} else if (state == OperatorState::NeedEnd) {
				// I'm waiting for TU but user is calling someone else - QSO failed
				setState(OperatorState::Failed);
			}
		}
	}

	if (contains(station_message::b4)) {
		if (state == OperatorState::NeedPrevEnd ||
		    state == OperatorState::NeedQso) {
			setState(OperatorState::NeedQso);
		} else if (state == OperatorState::NeedNr ||
		           state == OperatorState::NeedCall ||
		           state == OperatorState::NeedCallNr) {
			// User is telling someone else they worked them before - go quiet
			setState(OperatorState::NeedPrevEnd);
		} else if (state == OperatorState::NeedEnd) {
			// I'm the one being told B4 - fail the QSO
			setState(OperatorState::Failed);
		}
	}

	if (contains(station_message::nr)) {
		if (state == OperatorState::NeedQso) {
			// Number sent but I haven't been called - must be for someone else
			setState(OperatorState::NeedPrevEnd);
		} else if (state == OperatorState::NeedNr) {
			// I was called with exact match, now receiving number
			if (norepeats || rng->uniform() >= rptProb) {
				setState(OperatorState::NeedEnd);
			}
		}
		// Note: Don't process nr in NeedCallNr state (partial call match)
		// Station should just send their call again, not assume the nr is for them
	}

	if (contains(station_message::agn) || contains(station_message::nrqm)) {
		// User is asking for a repeat
		// Reset patience since they're actively working us
		patience = FULL_PATIENCE / 2;

		if (state == OperatorState::NeedEnd) {
			setState(OperatorState::NeedEnd);
		}
		// In NeedNr state, we haven't sent exchange yet, so no need to repeat
	}

	if (contains(station_message::tu)) {
		if (state == OperatorState::NeedPrevEnd) {
			// QSO finished, ready to call again
			setState(OperatorState::NeedQso);
		} else if (state == OperatorState::NeedEnd) {
			setState(OperatorState::Done);
		}
		// Note: stations in NeedNr/NeedCall/NeedCallNr shouldn't receive TU
		// because they should have already gone to NeedPrevEnd when user called someone else
	}

	// NeedQso timeout with no reply: retry the call (patience permitting)
	if (state == OperatorState::NeedQso && msgs.size() == 1 && contains(station_message::nomsg)) {
		decPatience();
		return;
	}

	// Non-lids give up on garbage
	if (!lids && msgs.size() == 1 && contains(station_message::nomsg)) {
		setState(OperatorState::NeedPrevEnd);
	}

	//if (state != OperatorState::NeedPrevEnd) {
	if (state == OperatorState::NeedPrevEnd) {
		decPatience();
	}
}

station_message DxOperator::getReply()
{
	switch (state) {
	case OperatorState::NeedPrevEnd:
	case OperatorState::Done:
	case OperatorState::Failed:
		return station_message::nomsg;

	case OperatorState::NeedQso:
		return station_message::mycall;

	case OperatorState::NeedNr:
		if (patience == (FULL_PATIENCE - 1) || rng->uniform() < 0.3) {
			return station_message::nrqm;
		} else {
			return station_message::agn;
		}
		break;


	case OperatorState::NeedCall:
		if (norepeats) {
			return station_message::demycallnr1;
		} else {
			double r1 = rng->uniform();
			if (r1 < 0.5) {
				return station_message::demycallnr1;
			} else if (r1 < 0.625) {
				return station_message::demycallnr2;
			} else {
				return station_message::mycallnr2;
			}
		}
		break;

	case OperatorState::NeedCallNr:
		if (norepeats || rng->uniform() < 0.5) {
			return station_message::demycall1;
		} else {
			return station_message::demycall2;
		}
		break;

	case OperatorState::NeedEnd:
		if (norepeats) {
			return station_message::r_nr;
		} else {
			if (patience == (FULL_PATIENCE - 1) || rng->uniform() < 0.9) {
				return station_message::r_nr;
			} else {
				return station_message::r_nr2;
			}
		}
		break;
	}

	return station_message::nomsg;
}
