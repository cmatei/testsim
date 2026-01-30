#include "contest.h"
#include "dxoper.h"
#include <rtaudio/RtAudio.h>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <fstream>

Contest::Contest(RNG *rng, const std::string &inifile)
	: me(nullptr), modulator(nullptr), _m5(nullptr),
	  _m1(nullptr), _m2(nullptr), _m3(nullptr),
	  _rng(rng), _keyer(nullptr), _callList(nullptr),
	  rtaudio(nullptr), bufcount(0), seconds(0.0),
	  _rfg0(1.0), _ritph(0.0), _extratime(0.0)
{
	// Try to read config file (default to contest.ini if not specified)
	std::string config_file = inifile.empty() ? "contest.ini" : inifile;
	bool config_loaded = false;

	try {
		readConfig(config_file);
		config_loaded = true;
		std::cout << "Loaded configuration from: " << config_file << std::endl;
	} catch (...) {
		// Fall through to defaults if file doesn't exist or has errors
		if (!inifile.empty()) {
			std::cerr << "Warning: Could not load " << config_file << ", using defaults" << std::endl;
		}
	}

	if (!config_loaded) {
		// Default configuration
		_rate = 11025;
		_bufsize = 512;
		_call = "P55CF";
		_wpm = 40;
		fast = 1.1;
		slow = 0.9;
		_bandwidth = 500;
		_pitch = 500;
		qsk = true;
		qskdecaytime = 0.030;
		rit = 0;
		monitor = 0.1;
		qrn = true;
		qrm = true;
		qsb = true;
		qsy = true;
		flutter = true;
		flutterProb = 0.3;
		lids = true;
		activity = 4;
		lidRstProb = 0.03;
		lidNrProb = 0.1;
		rptProb = 0.1;
		_tqrm = 240;
		duration = 60;
		mode = RunMode::pileup;
		_cwreverse = false;
		savewave = false;
		saveini = true;
		savesummary = true;
		fontsize = 12;
	}

	// Initialize audio processing components
	_qskdecayfactor = 1.0 / (_rate * qskdecaytime);
	_keyer = new Keyer(_rate, _bufsize);
	_callList = new CallList(rng);

	modulator = new Modulator(_bufsize, _rate, _pitch, _cwreverse);
	_m5 = new Agc(_bufsize);

	// Set bandwidth (this initializes _m1, _m2, _m3)
	setBandwidth(_bandwidth);

	// Create MyStation
	me = new MyStation(_rng, _keyer, this, _call, _pitch, _wpm, _bufsize, _rate);

	// Initialize QRM probability
	setTqrm(_tqrm);

	// Initialize RF gain arrays
	_rfg.resize(_bufsize + 1, 1.0);

	// Initialize buffer index for RIT
	_bufindex.resize(_bufsize);
	for (size_t i = 0; i < _bufsize; i++) {
		_bufindex[i] = static_cast<double>(i);
	}

	// Initialize RtAudio
	rtaudio = new RtAudio();
}

Contest::~Contest()
{
	stop();

	// Clean up stations
	for (auto s : stations) {
		delete s;
	}
	stations.clear();

	delete me;
	delete modulator;
	delete _m5;
	delete _m1;
	delete _m2;
	delete _m3;
	delete _keyer;
	delete _callList;
	delete rtaudio;
}

void Contest::setTqrm(float tqrm)
{
	_tqrm = tqrm;
	qrmProbPerBuffer = static_cast<float>(_bufsize) / _rate / _tqrm;
}

void Contest::setBandwidth(int bandwidth)
{
	_bandwidth = std::max(100, std::min(600, static_cast<int>(std::round(bandwidth / 50.0) * 50)));
	int navg = static_cast<int>(std::round(0.7 * _rate / _bandwidth));
	_fgain = std::sqrt(500.0 / _bandwidth);

	// Clean up old filters
	delete _m1;
	delete _m2;
	delete _m3;

	// Create new filters
	_m1 = new MovAvgComplex(_bufsize, navg);
	_m2 = new MovAvgComplex(_bufsize, navg);
	_m3 = new MovAvgComplex(_bufsize, navg);
}

void Contest::setPitch(float pitch)
{
	_pitch = pitch;
	if (modulator != nullptr) {
		modulator->setPitch(pitch);
	}
}

void Contest::setCall(const std::string &call)
{
	std::lock_guard<std::mutex> lock(audio_mutex);

	_call = call;
	if (me != nullptr) {
		me->mycall = call;
	}
}

void Contest::setWpm(int wpm)
{
	std::lock_guard<std::mutex> lock(audio_mutex);

	_wpm = wpm;
	if (me != nullptr) {
		me->wpm = wpm;
	}
}

void Contest::setCwreverse(bool cwreverse)
{
	_cwreverse = cwreverse;
	if (modulator != nullptr) {
		modulator->setReverse(cwreverse);
	}
}

double Contest::rfgfun(double a0, double a1)
{
	// Fast attack, slow decay for QSK RF gain
	if (a0 < a1) {
		a1 = a0 + _qskdecayfactor * (a1 - a0);
	}
	return a1;
}

void Contest::getAudio(float *outdata, unsigned int nframes)
{
	std::lock_guard<std::mutex> lock(audio_mutex);

	if (nframes != _bufsize) {
		std::cerr << "Warning: buffer size mismatch in getAudio" << std::endl;
		return;
	}

	bufcount++;
	checkDuration();

	const double NOISEAMP = 6000.0;

	// Generate complex white noise
	std::vector<std::complex<double>> reim(_bufsize);
	for (size_t i = 0; i < _bufsize; i++) {
		double r_real = _rng->uniform();
		double r_imag = _rng->uniform();
		reim[i] = std::complex<double>(
			-1.5 * NOISEAMP + 3.0 * NOISEAMP * r_real,
			-1.5 * NOISEAMP + 3.0 * NOISEAMP * r_imag
		);
	}

	// Add QRN (atmospheric noise spikes)
	if (qrn) {
		for (size_t i = 0; i < _bufsize; i++) {
			if (_rng->uniform() < 0.01) {
				reim[i] += 60.0 * NOISEAMP * (_rng->uniform() - 0.5);
			}
		}
		// Occasionally create a QRN station (burst)
		if (_rng->uniform() < 0.01) {
			stations.push_back(new QrnStation(_rng, _bufsize, _rate));
		}
	}

	// Maybe create QRM station
	if (qrm) {
		if (_rng->uniform() < qrmProbPerBuffer) {
			stations.push_back(new QrmStation(_rng, _keyer, _callList, me->mycall, _bufsize, _rate));
		}
	}

	// Mix in audio from all DX stations
	double ritfac = 2.0 * M_PI * rit / _rate;
	std::vector<station*> toRemove;

	for (auto s : stations) {
		if (s->state == station_state::deleteme) {
			// Check for QSY (station gave up)
			if (qsy && dynamic_cast<DxStation*>(s) != nullptr) {
				DxStation *dx = static_cast<DxStation*>(s);
				if (dx->oper->state != OperatorState::Done && dx->called) {
					qsoQueue.emplace(dx->mycall, 0, 0);
					// GUI notification would go here
				}
			}
			toRemove.push_back(s);
		} else if (s->state == station_state::sending) {
			const std::vector<float> &buf = s->get_buffer();
			const std::vector<float> &bfo = s->get_bfo();

			for (size_t i = 0; i < _bufsize && i < buf.size() && i < bfo.size(); i++) {
				double phase = static_cast<double>(bfo[i]) - _bufindex[i] * ritfac - _ritph;
				reim[i] += static_cast<double>(buf[i]) * std::exp(std::complex<double>(0, -phase));
			}
		}
	}

	// Update RIT phase
	_ritph += _bufsize * ritfac;
	_ritph = std::fmod(_ritph, 2.0 * M_PI);

	// Remove deleted stations
	for (auto s : toRemove) {
		stations.erase(std::remove(stations.begin(), stations.end(), s), stations.end());
		delete s;
	}

	// Handle MyStation audio with QSK or monitor
	double mvol = monitor * 20000.0;
	if (qsk) {
		if (me->state == station_state::sending) {
			const std::vector<float> &buf = me->get_buffer();
			_rfg[0] = _rfg0;
			for (size_t i = 0; i < _bufsize; i++) {
				_rfg[i + 1] = rfgfun(1.0 - buf[i], _rfg[i]);
				reim[i] = mvol * buf[i] * std::complex<double>(1, 1) + _rfg[i] * reim[i];
			}
			_rfg0 = _rfg[_bufsize];
		} else if (_rfg0 < 0.999) {
			_rfg[0] = _rfg0;
			for (size_t i = 0; i < _bufsize; i++) {
				_rfg[i + 1] = rfgfun(1.0, _rfg[i]);
				reim[i] = _rfg[i] * reim[i];
			}
			_rfg0 = _rfg[_bufsize];
		} else {
			_rfg0 = 1.0;
		}
	} else if (me->state == station_state::sending) {
		const std::vector<float> &buf = me->get_buffer();
		for (size_t i = 0; i < _bufsize; i++) {
			reim[i] = mvol * buf[i] * std::complex<double>(1, 1);
		}
	}

	// Apply bandwidth filtering (3-stage complex moving average)
	std::vector<std::complex<double>> filtered = _m1->avg(reim);
	filtered = _m2->avg(filtered);
	filtered = _m3->avg(filtered);

	// Apply filter gain
	for (auto &v : filtered) {
		v *= _fgain;
	}

	// Modulate to audio frequency
	std::vector<double> audio = modulator->modulate(filtered);

	// Apply AGC
	std::vector<double> agc_audio = _m5->process(audio);

	// Copy to output buffer
	for (size_t i = 0; i < _bufsize; i++) {
		outdata[i] = static_cast<float>(agc_audio[i]);
	}

	// Tick all stations
	me->tick();
	for (auto s : stations) {
		s->tick();
	}

	// Check for completed QSOs
	for (auto s : stations) {
		DxStation *dx = dynamic_cast<DxStation*>(s);
		if (dx != nullptr && dx->oper->state == OperatorState::Done) {
			auto qsoData = dx->dataToLastQso();
			qsoQueue.emplace(qsoData.call, qsoData.rst, qsoData.nr);
			// GUI notification would go here
		}
	}

	// Handle single station mode
	if (mode == RunMode::single || mode == RunMode::single_qsonr) {
		if (dxCount() == 0) {
			DxStation *s = new DxStation(_rng, _keyer, _callList, me,
				bufcount * _bufsize / (60.0 * _rate),
				lids, lidNrProb, lidRstProb, qsb, flutterProb,
				rptProb, fast, slow, true, _bufsize, _rate);
			stations.push_back(s);
			s->processEvent(station_event::mefinished);
		}
	}
}

int Contest::dxCount() const
{
	std::lock_guard<std::mutex> lock(audio_mutex);

	int count = 0;
	for (auto s : stations) {
		DxStation *dx = dynamic_cast<DxStation*>(s);
		if (dx != nullptr && dx->oper->state != OperatorState::Done) {
			count++;
		}
	}
	return count;
}

void Contest::onMeStartedSending()
{
	std::lock_guard<std::mutex> lock(audio_mutex);

	for (auto s : stations) {
		s->processEvent(station_event::mestarted);
	}
}

void Contest::onMeFinishedSending()
{
	std::lock_guard<std::mutex> lock(audio_mutex);

	// Create new calling stations in pileup mode
	if (mode != RunMode::single && mode != RunMode::single_qsonr) {
		bool should_create = false;
		for (auto msg : me->msgs) {
			if (msg == station_message::cq) {
				should_create = true;
				break;
			}
			if (msg == station_message::tu &&
			    std::find(me->msgs.begin(), me->msgs.end(), station_message::mycall) != me->msgs.end()) {
				should_create = true;
				break;
			}
		}

		if (should_create) {
			int newst = _rng->poisson(0.5 * activity);
			for (int i = 0; i < newst; i++) {
				stations.push_back(new DxStation(_rng, _keyer, _callList, me,
					bufcount * _bufsize / (60.0 * _rate),
					lids, lidNrProb, lidRstProb, qsb, flutterProb,
					rptProb, fast, slow, false, _bufsize, _rate));
			}
		}
	}

	// Notify all stations
	for (auto s : stations) {
		s->processEvent(station_event::mefinished);
	}
}

int Contest::audioCallback(void *outputBuffer, void *inputBuffer,
                           unsigned int nBufferFrames, double streamTime,
                           unsigned int status, void *userData)
{
	Contest *contest = static_cast<Contest*>(userData);
	float *outdata = static_cast<float*>(outputBuffer);

	if (status) {
		std::cerr << "RtAudio error: " << status << std::endl;
	}

	contest->getAudio(outdata, nBufferFrames);

	return 0;
}

void Contest::start()
{
	bufcount = 0;
	seconds = 0.0;

	if (rtaudio->getDeviceCount() < 1) {
		std::cerr << "No audio devices found!" << std::endl;
		return;
	}

	RtAudio::StreamParameters parameters;
	parameters.deviceId = rtaudio->getDefaultOutputDevice();
	parameters.nChannels = 1;
	parameters.firstChannel = 0;

	RtAudio::StreamOptions options;
	options.flags = RTAUDIO_MINIMIZE_LATENCY;

	unsigned int bufferFrames = _bufsize;

	try {
		rtaudio->openStream(&parameters, nullptr, RTAUDIO_FLOAT32,
		                    _rate, &bufferFrames, &Contest::audioCallback,
		                    static_cast<void*>(this), &options);
		rtaudio->startStream();
	} catch (RtAudioErrorType &e) {
		std::cerr << "RtAudio error on start" << std::endl;
	}
}

void Contest::stop()
{
	if (rtaudio && rtaudio->isStreamOpen()) {
		try {
			rtaudio->stopStream();
			rtaudio->closeStream();
		} catch (RtAudioErrorType &e) {
			std::cerr << "RtAudio error on stop" << std::endl;
		}
	}

	// Clear stations
	for (auto s : stations) {
		delete s;
	}
	stations.clear();
}

std::tuple<int, int, int> Contest::time() const
{
	std::lock_guard<std::mutex> lock(audio_mutex);
	int s = static_cast<int>(bufcount * _bufsize / _rate);
	int m = s / 60;
	s = s % 60;
	int h = m / 60;
	m = m % 60;
	return std::make_tuple(h, m, s);
}

void Contest::checkDuration()
{
	seconds = bufcount * _bufsize / static_cast<float>(_rate);

	if (mode == RunMode::single || mode == RunMode::pileup) {
		if (duration < seconds / 60.0) {
			// GUI notification: contest ended
			// me->app->contestEnded();
		}
	} else if (mode == RunMode::single_qsonr || mode == RunMode::pileup_qsonr) {
		// QSO-based duration checking would need GUI integration
		// This mode checks me->app->nrchecked >= duration
	}
}

void Contest::readConfig(const std::string &filename)
{
	std::ifstream file(filename);
	if (!file.is_open()) {
		throw std::runtime_error("Cannot open config file: " + filename);
	}

	std::string line, section;
	while (std::getline(file, line)) {
		// Skip comments and empty lines
		if (line.empty() || line[0] == '#') continue;

		// Check for section header
		if (line[0] == '[') {
			size_t end = line.find(']');
			if (end != std::string::npos) {
				section = line.substr(1, end - 1);
			}
			continue;
		}

		// Parse key=value
		size_t eq = line.find('=');
		if (eq == std::string::npos) continue;

		std::string key = line.substr(0, eq);
		std::string value = line.substr(eq + 1);

		// Trim whitespace
		key.erase(0, key.find_first_not_of(" \t"));
		key.erase(key.find_last_not_of(" \t") + 1);
		value.erase(0, value.find_first_not_of(" \t"));
		value.erase(value.find_last_not_of(" \t") + 1);

		// Parse based on section
		if (section == "Sound") {
			if (key == "rate") _rate = std::stoi(value);
			else if (key == "bufsize") _bufsize = std::stoi(value);
		} else if (section == "Station") {
			if (key == "call") _call = value;
			else if (key == "wpm") _wpm = std::stoi(value);
			else if (key == "fast") fast = std::stof(value);
			else if (key == "slow") slow = std::stof(value);
			else if (key == "bandwidth") _bandwidth = std::stoi(value);
			else if (key == "pitch") _pitch = std::stof(value);
			else if (key == "qsk") qsk = (std::stoi(value) != 0);
			else if (key == "qskdecaytime") qskdecaytime = std::stof(value);
			else if (key == "rit") rit = std::stoi(value);
			else if (key == "monitor") monitor = std::stof(value);
			else if (key == "cwreverse") _cwreverse = (std::stoi(value) != 0);
		} else if (section == "Conditions") {
			if (key == "qrn") qrn = (std::stoi(value) != 0);
			else if (key == "qrm") qrm = (std::stoi(value) != 0);
			else if (key == "qsb") qsb = (std::stoi(value) != 0);
			else if (key == "qsy") qsy = (std::stoi(value) != 0);
			else if (key == "flutter") flutter = (std::stoi(value) != 0);
			else if (key == "flutterprob") flutterProb = std::stof(value);
			else if (key == "lids") lids = (std::stoi(value) != 0);
			else if (key == "activity") activity = std::stoi(value);
			else if (key == "lidrstprob") lidRstProb = std::stof(value);
			else if (key == "lidnrprob") lidNrProb = std::stof(value);
			else if (key == "rptprob") rptProb = std::stof(value);
			else if (key == "tqrm") _tqrm = std::stof(value);
		} else if (section == "Contest") {
			if (key == "duration") duration = std::stoi(value);
			else if (key == "mode") {
				// Parse mode string
				if (value == "RunMode.stop") mode = RunMode::stop;
				else if (value == "RunMode.pileup") mode = RunMode::pileup;
				else if (value == "RunMode.single") mode = RunMode::single;
				else if (value == "RunMode.pileup_qsonr") mode = RunMode::pileup_qsonr;
				else if (value == "RunMode.single_qsonr") mode = RunMode::single_qsonr;
			}
			else if (key == "savewave") savewave = (std::stoi(value) != 0);
			else if (key == "saveini") saveini = (std::stoi(value) != 0);
			else if (key == "savesummary") savesummary = (std::stoi(value) != 0);
		} else if (section == "Appearance") {
			if (key == "fontsize") fontsize = std::stoi(value);
		}
	}
}

void Contest::writeConfig(const std::string &filename)
{
	std::ofstream file(filename);
	if (!file.is_open()) {
		throw std::runtime_error("Cannot write config file: " + filename);
	}

	file << "[Appearance]\n";
	file << "fontsize=" << fontsize << "\n\n";

	file << "[Sound]\n";
	file << "rate=" << _rate << "\n";
	file << "bufsize=" << _bufsize << "\n\n";

	file << "[Station]\n";
	file << "call=" << _call << "\n";
	file << "wpm=" << _wpm << "\n";
	file << "fast=" << fast << "\n";
	file << "slow=" << slow << "\n";
	file << "bandwidth=" << _bandwidth << "\n";
	file << "pitch=" << _pitch << "\n";
	file << "qsk=" << (qsk ? 1 : 0) << "\n";
	file << "qskdecaytime=" << qskdecaytime << "\n";
	file << "cwreverse=" << (_cwreverse ? 1 : 0) << "\n";
	file << "rit=" << rit << "\n";
	file << "monitor=" << monitor << "\n\n";

	file << "[Conditions]\n";
	file << "qrn=" << (qrn ? 1 : 0) << "\n";
	file << "qrm=" << (qrm ? 1 : 0) << "\n";
	file << "tqrm=" << _tqrm << "\n";
	file << "qsb=" << (qsb ? 1 : 0) << "\n";
	file << "flutter=" << (flutter ? 1 : 0) << "\n";
	file << "qsy=" << (qsy ? 1 : 0) << "\n";
	file << "lids=" << (lids ? 1 : 0) << "\n";
	file << "activity=" << activity << "\n";
	file << "lidrstprob=" << lidRstProb << "\n";
	file << "lidnrprob=" << lidNrProb << "\n";
	file << "rptprob=" << rptProb << "\n";
	file << "flutterprob=" << flutterProb << "\n\n";

	file << "[Contest]\n";
	file << "duration=" << duration << "\n";
	file << "mode=RunMode.";
	switch (mode) {
		case RunMode::stop: file << "stop"; break;
		case RunMode::pileup: file << "pileup"; break;
		case RunMode::single: file << "single"; break;
		case RunMode::pileup_qsonr: file << "pileup_qsonr"; break;
		case RunMode::single_qsonr: file << "single_qsonr"; break;
	}
	file << "\n";
	file << "savewave=" << (savewave ? 1 : 0) << "\n";
	file << "saveini=" << (saveini ? 1 : 0) << "\n";
	file << "savesummary=" << (savesummary ? 1 : 0) << "\n";
}
