#ifndef __MYSTATION_H
#define __MYSTATION_H

#include "station.h"
#include "keyer.h"
#include "random.h"
#include <string>
#include <vector>

// Forward declaration
class Contest;

/**
 * MyStation - the user's station
 *
 * Represents the operator's station in the contest simulator.
 * Handles sending messages in pieces with dynamic call sign updates.
 */
class MyStation : public station {
public:
	MyStation(RNG *rng, Keyer *keyer, Contest *contest,
	          const std::string &myCall, float pitch, int wpm,
	          size_t bufsize = 512, size_t rate = 11025);

	// Override base class methods
	void processEvent(station_event evt) override;
	const std::vector<float> &get_buffer();

	// MyStation specific methods
	void detectMessage(const std::string &msg);
	void sendText(const std::string &msg);
	void abortSend();
	bool updateCallInMessage(const std::string &call);

private:
	void sendNextPiece();

	Contest *contest;
	std::vector<std::string> pieces;  // Message pieces, '@' marks call placeholder
};

#endif
