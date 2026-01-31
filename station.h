#ifndef __STATION_H
#define __STATION_H

#include <string>
#include <vector>
#include <cstdint>

#include "keyer.h"
#include "random.h"

constexpr int32_t NEVER = INT32_MAX;

typedef enum {
	nomsg,
	cq,
	nr,
	tu,
	mycall,
	hiscall,
	b4,
	qm,
	nil,
	r_nr,
	r_nr2,
	demycall1,
	demycall2,
	demycallnr1,
	demycallnr2,
	nrqm,
	longcq,
	mycallnr2,
	qrl,
	qrl2,
	qsy,
	agn,
} station_message;

typedef enum {
	listening,
	copying,
	preparingtosend,
	sending,
	deleteme,
} station_state;


typedef enum {
	timeout,
	msgsent,
	mestarted,
	mefinished,
} station_event;


class station {
public:
	station(RNG *rng, Keyer *keyer_, size_t bufsize_ = 512, size_t rate_ = 11025);

	void set_pitch(float pitch_);
	float get_pitch() const { return pitch; }

	const std::vector<float> &get_bfo();
	const std::vector<float> &get_buffer();

	void sendText(const std::string &text);
	void sendMsg(station_message msg);

	virtual void processEvent(station_event event) = 0;
	void tick();

	std::string nrAsText();

	// Public data members accessed by derived classes and operators
	std::string hiscall, mycall;
	station_state state;
	std::vector<station_message> msgs;
	float wpm, amplitude;
	int rst, nr;
	int timeout;

protected:
	size_t bufsize, rate, sendpos;
	float pitch, dphi, fbfo;
	bool nr_err, standard_abbrevs;

	RNG *rng;
	Keyer *keyer;

	std::vector<float> envelope, bfo, buffer;

	std::string msgtext;
};

#endif
