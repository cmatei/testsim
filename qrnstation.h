#ifndef __QRNSTATION_H
#define __QRNSTATION_H

#include "station.h"
#include "random.h"

/**
 * QRN Station - simulates atmospheric noise (static crashes)
 *
 * QRN generates short bursts of random noise that simulate
 * atmospheric interference - the characteristic "crashes" and
 * "pops" heard on HF bands during thunderstorms.
 */
class QrnStation : public station {
public:
	QrnStation(RNG *rng, size_t bufsize = 512, size_t rate = 11025);

	// Override base class method
	void processEvent(station_event evt) override;
};

#endif
