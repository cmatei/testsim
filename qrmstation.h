#ifndef __QRMSTATION_H
#define __QRMSTATION_H

#include "station.h"
#include "keyer.h"
#include "calllist.h"
#include "random.h"
#include <vector>

/**
 * QRM Station - simulates interference from other stations
 *
 * QRM generates interference by sending CQ calls and other
 * messages at random intervals, simulating other stations
 * operating on or near the same frequency.
 */
class QrmStation : public station {
public:
	QrmStation(RNG *rng, Keyer *keyer, CallList *callList,
	           const std::string &hisCall = "",
	           size_t bufsize = 512, size_t rate = 11025);

	// Override base class method
	void processEvent(station_event evt) override;

private:
	// Static list of QRM messages
	static const std::vector<station_message> qrm_messages;

	int patience;  // Number of transmissions before giving up
	int b2, b6;    // Timeout bounds (2 and 6 seconds worth of buffers)
};

#endif
