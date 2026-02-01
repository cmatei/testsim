#include "contest.h"
#include "dxoper.h"
#include <rtaudio/RtAudio.h>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <ctime>
#include <cstring>

Contest::Contest(RNG *rng, const std::string &inifile)
	: me(nullptr), modulator(nullptr), _m5(nullptr),
	  _m1(nullptr), _m2(nullptr), _m3(nullptr),
	  _rng(rng), _keyer(nullptr), _callList(nullptr),
	  rtaudio(nullptr), bufcount(0), seconds(0.0),
	  _rfg0(1.0), _ritph(0.0), _extratime(0.0),
	  _samples_written(0)
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
		_rate = 44100;
		_bufsize = 2048;
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
		norepeats = 0;
		longnr = 0;
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

	// Pre-allocate audio processing buffers
	_reim.resize(_bufsize);
	_filtered.resize(_bufsize);
	_audio.resize(_bufsize);
	_agc_audio.resize(_bufsize);
	_toRemove.reserve(32);

	// Pre-allocate stations vector to avoid reallocation in audio thread
	stations.reserve(100);

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
	_m1 = new MovAvg(_bufsize, navg);
	_m2 = new MovAvg(_bufsize, navg);
	_m3 = new MovAvg(_bufsize, navg);
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
	std::lock_guard<std::recursive_mutex> lock(audio_mutex);

	_call = call;
	if (me != nullptr) {
		me->mycall = call;
	}
}

void Contest::setWpm(int wpm)
{
	std::lock_guard<std::recursive_mutex> lock(audio_mutex);

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
	// Fast attack: instantly mute when target drops below current
	if (a0 < a1)
		a1 = a0;
	// Slow decay: exponentially recover toward target
	return a1 + _qskdecayfactor * (a0 - a1);
}

void Contest::getAudio(float *outdata, unsigned int nframes)
{
	std::lock_guard<std::recursive_mutex> lock(audio_mutex);

	if (nframes != _bufsize) {
		std::cerr << "Warning: buffer size mismatch in getAudio: "
		          << nframes << " != " << _bufsize << std::endl;
		return;
	}

	bufcount++;
	checkDuration();

	const double NOISEAMP = 6000.0;

	// Generate complex white noise
	for (size_t i = 0; i < _bufsize; i++) {
		double r_real = _rng->uniform();
		double r_imag = _rng->uniform();
		_reim[i] = std::complex<double>(
			-1.5 * NOISEAMP + 3.0 * NOISEAMP * r_real,
			-1.5 * NOISEAMP + 3.0 * NOISEAMP * r_imag
		);
	}

	// Add QRN (atmospheric noise spikes)
	if (qrn) {
		for (size_t i = 0; i < _bufsize; i++) {
			if (_rng->uniform() < 0.01) {
				_reim[i] += 60.0 * NOISEAMP * (_rng->uniform() - 0.5);
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
	_toRemove.clear();

	for (auto s : stations) {
		if (s->state == station_state::deleteme) {
			// Check for QSY (station gave up)
			if (qsy && dynamic_cast<DxStation*>(s) != nullptr) {
				DxStation *dx = static_cast<DxStation*>(s);
				if (dx->oper->state != OperatorState::Done && dx->called) {
					qsoQueue.emplace(dx->mycall, 0, 0, 0);
					// GUI notification would go here
				}
			}
			_toRemove.push_back(s);
		} else if (s->state == station_state::sending) {
			const std::vector<float> &buf = s->get_buffer();
			const std::vector<float> &bfo = s->get_bfo();

			for (size_t i = 0; i < _bufsize && i < buf.size() && i < bfo.size(); i++) {
				double phase = static_cast<double>(bfo[i]) - _bufindex[i] * ritfac - _ritph;
				// Optimize: exp(i*(-phase)) = cos(-phase) + i*sin(-phase) = cos(phase) - i*sin(phase)
				double amplitude = static_cast<double>(buf[i]);
				double c = std::cos(phase);
				double s = std::sin(phase);
				_reim[i] += std::complex<double>(amplitude * c, -amplitude * s);
			}
		}
	}

	// Update RIT phase
	_ritph += _bufsize * ritfac;
	_ritph = std::fmod(_ritph, 2.0 * M_PI);

	// Remove deleted stations
	for (auto s : _toRemove) {
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
				_reim[i] = mvol * buf[i] * std::complex<double>(1, 1) + _rfg[i] * _reim[i];
			}
			_rfg0 = _rfg[_bufsize];
		} else if (_rfg0 < 0.999) {
			_rfg[0] = _rfg0;
			for (size_t i = 0; i < _bufsize; i++) {
				_rfg[i + 1] = rfgfun(1.0, _rfg[i]);
				_reim[i] = _rfg[i] * _reim[i];
			}
			_rfg0 = _rfg[_bufsize];
		} else {
			_rfg0 = 1.0;
		}
	} else {
		if (me->state == station_state::sending) {
			const std::vector<float> &buf = me->get_buffer();
			for (size_t i = 0; i < _bufsize; i++) {
				_reim[i] = mvol * buf[i] * std::complex<double>(1, 1);
			}
		}
	}

	// Apply bandwidth filtering (3-stage complex moving average)
	// Ping-pong between _reim and _filtered to avoid extra copies
	_m1->avg(_reim.data(), _filtered.data());
	_m2->avg(_filtered.data(), _reim.data());
	_m3->avg(_reim.data(), _filtered.data());

	// Apply filter gain
	for (size_t i = 0; i < _bufsize; i++) {
		_filtered[i] *= _fgain;
	}

	// Modulate to audio frequency
	modulator->modulate(_filtered.data(), _audio.data());

	// Apply AGC
	_m5->process(_audio.data(), _agc_audio.data());

	// Record to WAV file if enabled
	if (savewave) {
		writeAudioToWav(_agc_audio);
	}

	// Copy to output buffer
	for (size_t i = 0; i < _bufsize; i++) {
		outdata[i] = static_cast<float>(_agc_audio[i]);
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
			qsoQueue.emplace(qsoData.call, qsoData.rst, qsoData.nr, qsoData.wpm);
			// Log the QSO (only if nr > 0)
			logQso(qsoData.call, qsoData.rst, qsoData.nr, qsoData.wpm);
			// GUI notification would go here
		}
	}

	// Handle single station mode (defer creation to avoid heap allocation in audio callback)
	if (mode == RunMode::single || mode == RunMode::single_qsonr) {
		if (dxCount() == 0) {
			_pendingStations.store(1, std::memory_order_release);
			_pendingIsSingle.store(true, std::memory_order_release);
		}
	}
}

int Contest::dxCount() const
{
	// No lock: read-only from main thread, modified only in audio thread
	// Minor race acceptable for status display vs audio glitches from lock contention
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
	std::lock_guard<std::recursive_mutex> lock(audio_mutex);

	for (auto s : stations) {
		s->processEvent(station_event::mestarted);
	}
}

void Contest::onMeFinishedSending()
{
	std::lock_guard<std::recursive_mutex> lock(audio_mutex);

	// Defer station creation to main thread to avoid heap allocation in audio callback
	if (mode != RunMode::single && mode != RunMode::single_qsonr) {
		bool should_create = false;
		for (auto msg : me->msgs) {
			if (msg == station_message::cq) {
				should_create = true;
				break;
			}

#if 0
			if (msg == station_message::tu &&
			    std::find(me->msgs.begin(), me->msgs.end(), station_message::mycall) != me->msgs.end()) {
				should_create = true;
				break;
			}
#endif
		}

		if (should_create) {
			int newst = _rng->poisson(0.5 * activity);
			_pendingStations.store(newst, std::memory_order_release);
			_pendingIsSingle.store(false, std::memory_order_release);
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
	options.numberOfBuffers = 4;

	unsigned int bufferFrames = _bufsize;

	try {
		rtaudio->openStream(&parameters, nullptr, RTAUDIO_FLOAT32,
		                    _rate, &bufferFrames, &Contest::audioCallback,
		                    static_cast<void*>(this), &options);
		rtaudio->startStream();

		// Start WAV recording if enabled
		openWavFile();

		// Start QSO logging
		openLogFile();
	} catch (RtAudioErrorType &e) {
		std::cerr << "RtAudio error on start" << std::endl;
	}
}

void Contest::stop()
{
	if (rtaudio && rtaudio->isStreamOpen()) {
		try {
			// Stop WAV recording before stopping audio stream
			closeWavFile();

			// Close QSO log file
			closeLogFile();

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
	// No lock: read-only primitive access, modified only in audio thread
	// Minor race acceptable for status display vs audio glitches from lock contention
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
			else if (key == "norepeats") norepeats = std::stoi(value);
			else if (key == "longnr") longnr = std::stoi(value);
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
	file << "flutterprob=" << flutterProb << "\n";
	file << "norepeats=" << norepeats << "\n";
	file << "longnr=" << longnr << "\n\n";

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

// WAV recording implementation

void Contest::openWavFile()
{
	if (!savewave) return;

	// Generate timestamp-based filename
	std::time_t now = ::time(nullptr);
	struct tm *timeinfo = ::localtime(&now);
	char timestamp[32];
	std::strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", timeinfo);

	_wavfilename = std::string("contest_") + timestamp + ".wav";

	// Open file in binary mode
	_wavfile.open(_wavfilename, std::ios::binary);

	if (!_wavfile.is_open()) {
		std::cerr << "Error: Could not open WAV file " << _wavfilename << std::endl;
		savewave = false;  // Disable recording on failure
		return;
	}

	// Write placeholder header (44 bytes)
	// Will be rewritten with correct size in closeWavFile()
	char header[44] = {0};
	_wavfile.write(header, 44);

	_samples_written = 0;

	std::cout << "Recording audio to: " << _wavfilename << std::endl;
}

void Contest::writeAudioToWav(const std::vector<double> &audio)
{
	if (!_wavfile.is_open()) return;

	// Convert double samples to 16-bit PCM
	// Audio range: approximately [-1.0, 1.0] after AGC
	// Map to int16 range: [-32768, 32767]

	for (size_t i = 0; i < audio.size(); i++) {
		// Clamp to prevent overflow
		double sample = std::clamp(audio[i], -1.0, 1.0);

		// Scale to int16 range
		int16_t pcm_sample = static_cast<int16_t>(sample * 32767.0);

		// Write as little-endian (standard for WAV)
		char bytes[2];
		bytes[0] = pcm_sample & 0xFF;         // Low byte
		bytes[1] = (pcm_sample >> 8) & 0xFF;  // High byte

		_wavfile.write(bytes, 2);
	}

	_samples_written += audio.size();
}

void Contest::closeWavFile()
{
	if (!_wavfile.is_open()) return;

	// Flush any buffered data
	_wavfile.flush();

	// Rewrite header with correct sizes
	writeWavHeader(_samples_written);

	_wavfile.close();

	std::cout << "Recording stopped: " << _wavfilename
	          << " (" << _samples_written << " samples, "
	          << (_samples_written / static_cast<double>(_rate)) << " seconds)" << std::endl;
}

void Contest::writeWavHeader(size_t num_samples)
{
	if (!_wavfile.is_open()) return;

	// Seek to beginning of file
	_wavfile.seekp(0, std::ios::beg);

	const uint16_t num_channels = 1;
	const uint16_t bits_per_sample = 16;
	const uint32_t sample_rate = static_cast<uint32_t>(_rate);
	const uint32_t byte_rate = sample_rate * num_channels * bits_per_sample / 8;
	const uint16_t block_align = num_channels * bits_per_sample / 8;
	const uint32_t data_size = num_samples * num_channels * bits_per_sample / 8;
	const uint32_t chunk_size = 36 + data_size;

	// Helper lambda for writing little-endian values
	auto write_u32 = [this](uint32_t value) {
		char bytes[4];
		bytes[0] = value & 0xFF;
		bytes[1] = (value >> 8) & 0xFF;
		bytes[2] = (value >> 16) & 0xFF;
		bytes[3] = (value >> 24) & 0xFF;
		_wavfile.write(bytes, 4);
	};

	auto write_u16 = [this](uint16_t value) {
		char bytes[2];
		bytes[0] = value & 0xFF;
		bytes[1] = (value >> 8) & 0xFF;
		_wavfile.write(bytes, 2);
	};

	// RIFF header
	_wavfile.write("RIFF", 4);
	write_u32(chunk_size);
	_wavfile.write("WAVE", 4);

	// fmt subchunk
	_wavfile.write("fmt ", 4);
	write_u32(16);  // Subchunk size for PCM
	write_u16(1);   // Audio format (1 = PCM)
	write_u16(num_channels);
	write_u32(sample_rate);
	write_u32(byte_rate);
	write_u16(block_align);
	write_u16(bits_per_sample);

	// data subchunk
	_wavfile.write("data", 4);
	write_u32(data_size);

	// Header is now 44 bytes total
}

// QSO logging implementation

void Contest::openLogFile()
{
	// Generate timestamp-based filename (same timestamp format as WAV file)
	std::time_t now = ::time(nullptr);
	struct tm *timeinfo = ::localtime(&now);
	char timestamp[32];
	std::strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", timeinfo);

	_logfilename = std::string("contest_") + timestamp + ".log";

	// Open file in text mode
	_logfile.open(_logfilename, std::ios::out);

	if (!_logfile.is_open()) {
		std::cerr << "Error: Could not open log file " << _logfilename << std::endl;
		return;
	}

	// Write CSV header
	_logfile << "UTC Time,Callsign,Serial Number" << std::endl;

	std::cout << "Logging QSOs to: " << _logfilename << std::endl;
}

void Contest::logQso(const std::string &call, int rst, int nr, int wpm)
{
	// Only log valid QSOs (non-zero serial numbers)
	if (nr == 0 || !_logfile.is_open()) {
		return;
	}

	// Get current UTC time
	std::time_t now = ::time(nullptr);
	struct tm *timeinfo = ::gmtime(&now);  // Use gmtime for UTC
	char timestr[32];
	std::strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", timeinfo);

	// Write log entry: UTC timestamp, callsign, serial number
	_logfile << timestr << "," << call << "," << nr << std::endl;
	_logfile.flush();  // Ensure it's written immediately
}

void Contest::closeLogFile()
{
	if (_logfile.is_open()) {
		_logfile.close();
		std::cout << "QSO log closed: " << _logfilename << std::endl;
	}
}

// Thread-safe wrappers for MyStation access from main thread

void Contest::sendText(const std::string &msg)
{
	std::lock_guard<std::recursive_mutex> lock(audio_mutex);
	me->sendText(msg);
}

void Contest::abortSend()
{
	std::lock_guard<std::recursive_mutex> lock(audio_mutex);
	me->abortSend();
}

bool Contest::updateCallInMessage(const std::string &call)
{
	std::lock_guard<std::recursive_mutex> lock(audio_mutex);
	return me->updateCallInMessage(call);
}

bool Contest::isSending() const
{
	// No lock: read-only primitive access, modified only in audio thread
	// Minor race acceptable for status display vs audio glitches from lock contention
	return me->state == station_state::sending;
}

bool Contest::hasQso() const
{
	// No lock: read-only check, modified only in audio thread
	// Minor race acceptable for status display vs audio glitches from lock contention
	return !qsoQueue.empty();
}

std::tuple<std::string, int, int, int> Contest::popQso()
{
	std::lock_guard<std::recursive_mutex> lock(audio_mutex);
	auto qso = qsoQueue.front();
	qsoQueue.pop();
	return qso;
}

void Contest::createPendingStations()
{
	// Check if there are pending stations to create (lock-free read)
	int count = _pendingStations.load(std::memory_order_acquire);
	if (count == 0) {
		return;
	}

	// Reset pending count
	_pendingStations.store(0, std::memory_order_release);
	bool is_single = _pendingIsSingle.load(std::memory_order_acquire);

	// Now create stations with mutex held
	std::lock_guard<std::recursive_mutex> lock(audio_mutex);

	for (int i = 0; i < count; i++) {
		DxStation *s = new DxStation(_rng, _keyer, _callList, me,
			bufcount * _bufsize / (60.0 * _rate),
			lids, lidNrProb, lidRstProb, qsb, flutterProb,
			rptProb, fast, slow, is_single, norepeats, longnr, _bufsize, _rate);
		stations.push_back(s);

		// Trigger newly created stations to start calling
		// (They missed the mefinished event that was sent earlier in onMeFinishedSending)
		s->processEvent(station_event::mefinished);
	}
}
