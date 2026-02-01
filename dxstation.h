#ifndef __DXSTATION_H
#define __DXSTATION_H

#include "station.h"
#include "dxoper.h"
#include "qsb.h"
#include "calllist.h"

class DxStation : public station {
public:
	DxStation(RNG *rng, Keyer *keyer, CallList *callList, station *cqstn,
	          int minutes = 0, bool lids = true, double lidNrProb = 0.1,
	          double lidRstProb = 0.03, bool qsb = true, double flutterProb = 0.3,
	          double rptProb = 0.1, double fast = 1.1, double slow = 0.9,
	          bool isSingle = false, int norepeats = 0, size_t bufsize = 512, size_t rate = 11025);

	~DxStation();

	// Override base class methods
	void processEvent(station_event evt) override;
	const std::vector<float> &get_buffer();

	// DxStation specific methods
	struct QsoData {
		std::string call;
		int rst;
		int nr;
		int wpm;
	};
	QsoData dataToLastQso();

	bool called;  // Has transmitted at least once
	DxOperator *oper;

private:
	station *cqstn;
	QSB *qsb_effect;
	int rst_to_send;
};

#endif
