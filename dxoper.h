#ifndef __DXOPER_H
#define __DXOPER_H

#include <string>
#include <vector>
#include "station.h"
#include "random.h"

// Forward declaration
class station;

// My call correctness enumeration
enum class MyCallCorrect {
	Yes,
	No,
	Almost
};

// Operator state machine states
enum class OperatorState {
	NeedPrevEnd,  // Waiting for previous QSO to end
	NeedQso,      // Ready to start QSO (will send my call)
	NeedNr,       // Waiting to receive exchange number
	NeedCall,     // Need to send my call again
	NeedCallNr,   // Need to send both call and nr
	NeedEnd,      // Waiting for final confirmation (TU)
	Done,         // QSO completed successfully
	Failed        // QSO failed, give up
};

class DxOperator {
public:
	static constexpr int FULL_PATIENCE = 8;

	DxOperator(RNG *rng, int minutes, station *cqstn, const std::string &call,
	           int skills, double s2bfac, bool lids, double rptProb,
	           int wpm, double fast, double slow, bool isSingle, int norepeats, int longnr,
	           OperatorState state = OperatorState::NeedPrevEnd);

	// Timing methods
	int getSendDelay();
	int getWpm();
	int getNr();
	int getReplyTimeout();

	// State machine methods
	void msgReceived(const std::vector<station_message> &msgs);
	station_message getReply();

	// State accessors
	OperatorState getState() const { return state; }

	// Public for DxStation access
	OperatorState state;
	int patience;
	int repeatCnt;

private:
	void setState(OperatorState newState);
	void decPatience();
	MyCallCorrect isMyCall();

	RNG *rng;
	station *cqstn;
	std::string myCall;
	int skills;
	int minutes;
	bool isSingle;
	bool lids;
	int wpm;
	double slow;
	double fast;
	double rptProb;
	double s2bfac;
	int norepeats;  // 0=normal (varied messages, may repeat), 1=no repeats (shortest messages)
	int longnr;     // 0=time-based serials, 1=random 4-digit serials
};

#endif
