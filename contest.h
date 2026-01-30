#ifndef __CONTEST_H
#define __CONTEST_H

#include "station.h"
#include "mystation.h"
#include "dxstation.h"
#include "qrnstation.h"
#include "qrmstation.h"
#include "keyer.h"
#include "random.h"
#include "calllist.h"
#include "audioprocess.h"
#include "qsb.h"
#include <vector>
#include <queue>
#include <string>
#include <complex>
#include <memory>

// Forward declaration for RtAudio
class RtAudio;

enum class RunMode {
	stop = 1,
	pileup = 2,
	single = 3,
	pileup_qsonr = 4,
	single_qsonr = 5
};

/**
 * Contest - Main orchestrator for the contest simulator
 *
 * Manages all stations, mixes audio, handles RtAudio streaming,
 * creates/removes stations based on activity, and processes events.
 */
class Contest {
public:
	Contest(RNG *rng, const std::string &inifile = "");
	~Contest();

	// Start/stop audio streaming
	void start();
	void stop();

	// Station management
	int dxCount() const;
	void onMeStartedSending();
	void onMeFinishedSending();

	// Time tracking
	std::tuple<int, int, int> time() const;
	void checkDuration();

	// Configuration
	void readConfig(const std::string &filename);
	void writeConfig(const std::string &filename);

	// Properties with getters/setters
	float getTqrm() const { return _tqrm; }
	void setTqrm(float tqrm);

	int getBandwidth() const { return _bandwidth; }
	void setBandwidth(int bandwidth);

	float getPitch() const { return _pitch; }
	void setPitch(float pitch);

	std::string getCall() const { return _call; }
	void setCall(const std::string &call);

	int getWpm() const { return _wpm; }
	void setWpm(int wpm);

	bool getCwreverse() const { return _cwreverse; }
	void setCwreverse(bool cwreverse);

	// Public members for GUI access
	MyStation *me;
	std::vector<station*> stations;
	std::queue<std::tuple<std::string, int, int>> qsoQueue;

	// Configuration parameters
	int wpm;
	float fast;
	float slow;
	float pitch;
	bool qsk;
	float qskdecaytime;
	int rit;
	float monitor;
	bool qrn;
	bool qrm;
	bool qsb;
	bool qsy;
	bool flutter;
	float flutterProb;
	bool lids;
	int activity;
	float lidRstProb;
	float lidNrProb;
	float rptProb;
	float tqrm;
	int duration;
	RunMode mode;
	bool cwreverse;
	bool savewave;
	bool saveini;
	bool savesummary;
	int fontsize;

	size_t bufcount;
	float seconds;

private:
	// Audio callback (static wrapper for RtAudio)
	static int audioCallback(void *outputBuffer, void *inputBuffer,
	                         unsigned int nBufferFrames, double streamTime,
	                         unsigned int status, void *userData);

	// Instance audio processing method
	void getAudio(float *outdata, unsigned int nframes);

	// RF gain function for QSK
	double rfgfun(double a0, double a1);

	// Audio processing components
	RNG *_rng;
	Keyer *_keyer;
	CallList *_callList;
	Modulator *modulator;
	Agc *_m5;
	MovAvgComplex *_m1;
	MovAvgComplex *_m2;
	MovAvgComplex *_m3;

	// Audio parameters
	size_t _rate;
	size_t _bufsize;

	// Configuration
	std::string _call;
	int _wpm;
	int _bandwidth;
	float _pitch;
	float _tqrm;
	bool _cwreverse;

	// Audio processing state
	float _qskdecayfactor;
	float qrmProbPerBuffer;
	float _fgain;
	double _rfg0;
	std::vector<double> _rfg;
	double _ritph;
	std::vector<double> _bufindex;
	float _extratime;

	// RtAudio handle
	RtAudio *rtaudio;
};

#endif
