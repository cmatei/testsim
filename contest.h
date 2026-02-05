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
#include <mutex>
#include <atomic>
#include <fstream>

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
	double getTqrm() const { return _tqrm; }
	void setTqrm(double tqrm);

	int getBandwidth() const { return _bandwidth; }
	void setBandwidth(int bandwidth);

	double getPitch() const { return _pitch; }
	void setPitch(double pitch);

	std::string getCall() const { return _call; }
	void setCall(const std::string &call);

	int getWpm() const { return _wpm; }
	void setWpm(int wpm);

	bool getCwreverse() const { return _cwreverse; }
	void setCwreverse(bool cwreverse);

	// Thread-safe wrappers for MyStation (lock audio_mutex before delegating)
	void sendText(const std::string &msg);
	void abortSend();
	bool updateCallInMessage(const std::string &call);
	bool isSending() const;

	// Thread-safe QSO queue access
	bool hasQso() const;
	std::tuple<std::string, int, int, int> popQso();

	// Deferred station creation (to avoid allocation in audio thread)
	void createPendingStations();

	// Public members (access only through wrappers from main thread)
	MyStation *me;
	std::vector<station*> stations;
	std::queue<std::tuple<std::string, int, int, int>> qsoQueue;

	// Configuration parameters
	int wpm;
	double fast;
	double slow;
	double pitch;
	bool qsk;
	double qskdecaytime;
	int rit;
	double monitor;
	bool qrn;
	bool qrm;
	bool qsb;
	bool qsy;
	bool flutter;
	double flutterProb;
	bool lids;
	int activity;
	double lidRstProb;
	double lidNrProb;
	double rptProb;
	double tqrm;
	int duration;
	int norepeats;
	int longnr;
	RunMode mode;
	bool cwreverse;
	bool savewave;
	bool saveini;
	bool savesummary;
	int fontsize;

	// Network protocol ports (0 = disabled)
	int winkeyer_port;
	int cwdaemon_port;
	int rigctl_port;

	size_t bufcount;
	double seconds;

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
	MovAvg *_m1;
	MovAvg *_m2;
	MovAvg *_m3;

	// Audio parameters
	size_t _rate;
	size_t _bufsize;

	// Configuration
	std::string _call;
	int _wpm;
	int _bandwidth;
	double _pitch;
	double _tqrm;
	bool _cwreverse;

	// Audio processing state
	double _qskdecayfactor;
	double qrmProbPerBuffer;
	double _fgain;
	double _rfg0;
	std::vector<double> _rfg;
	double _ritph;
	std::vector<double> _bufindex;
	double _extratime;

	// Pre-allocated audio buffers (avoid heap allocation in callback)
	std::vector<std::complex<double>> _reim;
	std::vector<std::complex<double>> _filtered;
	std::vector<double> _audio;
	std::vector<double> _agc_audio;
	std::vector<station*> _toRemove;

	// Deferred station creation (avoid heap allocation in audio callback)
	std::atomic<int> _pendingStations{0};
	std::atomic<bool> _pendingIsSingle{false};

	// RtAudio handle
	RtAudio *rtaudio;

	// Thread synchronization for audio callback (mutable for const member functions)
	mutable std::recursive_mutex audio_mutex;

	// WAV recording state
	std::ofstream _wavfile;
	std::string _wavfilename;
	size_t _samples_written;

	// WAV recording helper methods
	void openWavFile();
	void closeWavFile();
	void writeWavHeader(size_t num_samples);
	void writeAudioToWav(const std::vector<double> &audio);

	// QSO logging state
	std::ofstream _logfile;
	std::string _logfilename;

	// QSO logging helper methods
	void openLogFile();
	void closeLogFile();
	void logQso(const std::string &call, int rst, int nr, int wpm);
};

#endif
