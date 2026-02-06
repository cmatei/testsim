#include <iostream>
#include <string>
#include <csignal>
#include <unistd.h>
#include <memory>

#include "contest.h"
#include "winkeyer.h"
#include "random.h"

// Global pointer for signal handler
Contest *g_contest = nullptr;
bool g_running = true;

void signal_handler(int signum)
{
	std::cout << "\nReceived signal " << signum << ", shutting down..." << std::endl;
	g_running = false;
	if (g_contest) {
		g_contest->stop();
	}
}

void print_usage(const char *prog)
{
	std::cout << "Contest Simulator - Network Keyer Mode\n";
	std::cout << "Usage: " << prog << " [options]\n\n";
	std::cout << "Options:\n";
	std::cout << "  --winkeyer-port <port>               TCP port for WinKeyer (0=disabled)\n";
	std::cout << "  --cwdaemon-port <port>               UDP port for cwdaemon (0=disabled)\n";
	std::cout << "  --rigctl-port <port>                 TCP port for rigctld (0=disabled)\n";
	std::cout << "  --rigctl-verbose                     Enable verbose logging for rigctld\n";
	std::cout << "  --winkeyer-version <2|3>             WinKeyer protocol version (default: 3)\n";
	std::cout << "  --config <file>                      Configuration file (default: contest.ini)\n";
	std::cout << "  --help                               Show this help\n\n";
	std::cout << "Protocols:\n";
	std::cout << "  WinKeyer  - WinKeyer2/3 over TCP (N1MM+, Win-Test, etc.)\n";
	std::cout << "  cwdaemon  - cwdaemon over UDP (TLF, xlog, etc.)\n";
	std::cout << "  rigctld   - hamlib rigctld over TCP (rig control, RIT)\n\n";
	std::cout << "Port Configuration:\n";
	std::cout << "  Ports can be set via command line or in contest.ini [Network] section:\n";
	std::cout << "    winkeyer_port=7890   (0 to disable)\n";
	std::cout << "    cwdaemon_port=6789   (0 to disable)\n";
	std::cout << "    rigctl_port=4532     (0 to disable)\n";
	std::cout << "  Command line arguments override config file values.\n\n";
	std::cout << "Examples:\n";
	std::cout << "  " << prog << " --winkeyer-port 7890\n";
	std::cout << "  " << prog << " --cwdaemon-port 6789\n";
	std::cout << "  " << prog << " --winkeyer-port 7890 --cwdaemon-port 6789 --rigctl-port 4532\n";
	std::cout << "  " << prog << " --config contest.ini\n\n";
	std::cout << "Audio Routing:\n";
	std::cout << "  Configure your logger to use 'pulse' or system default audio input\n";
	std::cout << "  The simulator will output to the default audio device\n\n";
}

void print_status(Contest *contest, WinKeyerServer *wk, CwdaemonServer *cw)
{
	auto [h, m, s] = contest->time();
	int dx_count = contest->dxCount();

	int wpm = 0;
	if (wk) wpm = wk->getWpm();
	else if (cw) wpm = cw->getWpm();

	std::cout << "\r["
	          << (h < 10 ? "0" : "") << h << ":"
	          << (m < 10 ? "0" : "") << m << ":"
	          << (s < 10 ? "0" : "") << s << "] "
	          << "DX: " << dx_count << " "
	          << "WPM: " << wpm << " "
	          << (contest->isSending() ? "TX" : "RX")
	          << "          " << std::flush;
}

int main(int argc, char **argv)
{
	int winkeyer_port = -1;  // -1 means use config file value
	int cwdaemon_port = -1;  // -1 means use config file value
	int rigctl_port = -1;  // -1 means use config file value
	bool rigctl_verbose = false;
	bool rigctl_verbose_set = false;  // Track if set via command line
	int winkeyer_version = 3;  // Default WinKeyer3
	std::string config_file;

	// Parse command line arguments
	for (int i = 1; i < argc; i++) {
		std::string arg = argv[i];
		if (arg == "--help" || arg == "-h") {
			print_usage(argv[0]);
			return 0;
		} else if (arg == "--winkeyer-port" && i + 1 < argc) {
			winkeyer_port = std::stoi(argv[++i]);
		} else if (arg == "--cwdaemon-port" && i + 1 < argc) {
			cwdaemon_port = std::stoi(argv[++i]);
		} else if (arg == "--rigctl-port" && i + 1 < argc) {
			rigctl_port = std::stoi(argv[++i]);
		} else if (arg == "--rigctl-verbose") {
			rigctl_verbose = true;
			rigctl_verbose_set = true;
		} else if (arg == "--winkeyer-version" && i + 1 < argc) {
			winkeyer_version = std::stoi(argv[++i]);
			if (winkeyer_version != 2 && winkeyer_version != 3) {
				std::cerr << "Error: WinKeyer version must be 2 or 3" << std::endl;
				return 1;
			}
		} else if (arg == "--config" && i + 1 < argc) {
			config_file = argv[++i];
		} else {
			std::cerr << "Unknown option: " << arg << std::endl;
			print_usage(argv[0]);
			return 1;
		}
	}

	std::cout << "=====================================\n";
	std::cout << "      Contest Simulator\n";
	std::cout << "=====================================\n\n";

	// Setup signal handlers
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	// Create RNG
	RNG rng;

	// Create Contest
	std::cout << "Initializing contest simulator...\n";
	Contest contest(&rng, config_file);
	g_contest = &contest;

	// Use config file port values for any not specified on command line
	if (winkeyer_port == -1) winkeyer_port = contest.winkeyer_port;
	if (cwdaemon_port == -1) cwdaemon_port = contest.cwdaemon_port;
	if (rigctl_port == -1) rigctl_port = contest.rigctl_port;
	if (!rigctl_verbose_set) rigctl_verbose = contest.rigctl_verbose;

	// Check that at least one protocol is enabled
	if (winkeyer_port == 0 && cwdaemon_port == 0 && rigctl_port == 0) {
		std::cerr << "Error: No network protocols enabled!\n";
		std::cerr << "Enable at least one protocol via command line or config file.\n";
		std::cerr << "Use --help for usage information.\n";
		return 1;
	}

	std::cout << "Configuration:\n";
	std::cout << "  Call:      " << contest.getCall() << "\n";
	std::cout << "  WPM:       " << contest.getWpm() << "\n";
	std::cout << "  Pitch:     " << contest.getPitch() << " Hz\n";
	std::cout << "  Bandwidth: " << contest.getBandwidth() << " Hz\n";
	std::cout << "  Mode:      ";
	switch (contest.mode) {
		case RunMode::pileup: std::cout << "Pileup"; break;
		case RunMode::single: std::cout << "Single"; break;
		case RunMode::pileup_qsonr: std::cout << "Pileup (QSO count)"; break;
		case RunMode::single_qsonr: std::cout << "Single (QSO count)"; break;
		default: std::cout << "Unknown"; break;
	}
	std::cout << "\n";
	std::cout << "  Activity:  " << contest.activity << "\n";
	std::cout << "  QRN:       " << (contest.qrn ? "ON" : "OFF") << "\n";
	std::cout << "  QRM:       " << (contest.qrm ? "ON" : "OFF") << "\n";
	std::cout << "  QSB:       " << (contest.qsb ? "ON" : "OFF") << "\n";
	std::cout << "  Duration:  " << contest.duration << " minutes\n\n";

	// Create servers based on port configuration (0 = disabled)
	std::unique_ptr<WinKeyerServer> winkeyer;
	std::unique_ptr<CwdaemonServer> cwdaemon;
	std::unique_ptr<RigctldServer> rigctl;

	if (winkeyer_port > 0) {
		std::cout << "Starting WinKeyer" << winkeyer_version << " TCP server on port " << winkeyer_port << "\n";
		winkeyer = std::make_unique<WinKeyerServer>(winkeyer_port, winkeyer_version);

		if (!winkeyer->isOpen()) {
			std::cerr << "\nError: Could not create TCP listener on port " << winkeyer_port << "\n";
			std::cerr << "Make sure the port is not already in use.\n";
			return 1;
		}

		// Connect WinKeyer callbacks
		winkeyer->onTextToSend = [&contest](const std::string &text) {
			std::string upper = text;
			for (auto &c : upper) {
				c = std::toupper(c);
			}
			contest.sendText(upper);
		};

		winkeyer->onSpeedChange = [&contest](int wpm) {
			contest.setWpm(wpm);
		};

		winkeyer->onPttChange = [](bool ptt) {
			// PTT changes handled automatically by MyStation state
		};
	}

	if (cwdaemon_port > 0) {
		std::cout << "Starting cwdaemon UDP server on port " << cwdaemon_port << "\n";
		cwdaemon = std::make_unique<CwdaemonServer>(cwdaemon_port);

		if (!cwdaemon->isOpen()) {
			std::cerr << "\nError: Could not create UDP socket on port " << cwdaemon_port << "\n";
			std::cerr << "Make sure the port is not already in use.\n";
			return 1;
		}

		// Connect cwdaemon callbacks
		cwdaemon->onTextToSend = [&contest](const std::string &text) {
			std::string upper = text;
			for (auto &c : upper) {
				c = std::toupper(c);
			}
			contest.sendText(upper);
		};

		cwdaemon->onSpeedChange = [&contest](int wpm) {
			contest.setWpm(wpm);
		};

		cwdaemon->onPttChange = [](bool ptt) {
			// PTT changes handled automatically by MyStation state
		};

		cwdaemon->onAbort = [&contest]() {
			contest.abortSend();
		};
	}

	if (rigctl_port > 0) {
		std::cout << "Starting rigctld TCP server on port " << rigctl_port << "\n";
		rigctl = std::make_unique<RigctldServer>(rigctl_port, rigctl_verbose);

		if (!rigctl->isOpen()) {
			std::cerr << "\nError: Could not create TCP listener on port " << rigctl_port << "\n";
			std::cerr << "Make sure the port is not already in use.\n";
			return 1;
		}

		// Wire up callbacks - RIT is bidirectional
		rigctl->onGetRit = [&contest]() -> int {
			return contest.rit;
		};

		rigctl->onSetRit = [&contest](int rit_hz) {
			contest.rit = rit_hz;
		};

		// Other callbacks return simulated state
		rigctl->onGetFreq = [&rigctl]() -> long long {
			return rigctl->freq_hz;
		};

		rigctl->onGetMode = [&rigctl]() -> std::string {
			return rigctl->mode;
		};

		rigctl->onGetPassband = [&rigctl]() -> int {
			return rigctl->passband_hz;
		};

		rigctl->onGetVfo = [&rigctl]() -> std::string {
			return rigctl->vfo;
		};

		rigctl->onSetFreq = [&rigctl](long long freq_hz) {
			rigctl->freq_hz = freq_hz;
		};

		rigctl->onSetMode = [&rigctl](const std::string& mode, int passband_hz) {
			rigctl->mode = mode;
			rigctl->passband_hz = passband_hz;
		};

		rigctl->onSetVfo = [&rigctl](const std::string& vfo) {
			rigctl->vfo = vfo;
		};
	}

	// Track MyStation state for WinKeyer busy status
	bool last_sending = contest.isSending();

	// Start audio
	std::cout << "Starting audio output...\n";
	contest.start();

	std::cout << "\n=== Contest Running ===\n";
	if (winkeyer) {
		std::cout << "WinKeyer" << winkeyer_version << " listening on: localhost:" << winkeyer_port << "\n";
	}
	if (cwdaemon) {
		std::cout << "cwdaemon listening on: localhost:" << cwdaemon_port << "\n";
	}
	if (rigctl) {
		std::cout << "rigctld listening on: localhost:" << rigctl_port << "\n";
	}
	std::cout << "Press Ctrl+C to stop\n\n";

	// Main loop
	int status_counter = 0;
	while (g_running) {
		// Poll servers for incoming commands
		if (winkeyer) {
			winkeyer->poll();
		}
		if (cwdaemon) {
			cwdaemon->poll();
		}
		if (rigctl) {
			rigctl->poll();
		}

		// Update busy status if MyStation state changed (WinKeyer only)
		if (winkeyer) {
			bool is_sending = contest.isSending();
			if (is_sending != last_sending) {
				winkeyer->setBusy(is_sending);
				last_sending = is_sending;
			}
		}

		// Print status every ~100ms
		if (++status_counter >= 10) {
			print_status(&contest, winkeyer.get(), cwdaemon.get());
			status_counter = 0;
		}

		// Create any pending stations (deferred from audio callback)
		contest.createPendingStations();

		// Check for completed QSOs
		while (contest.hasQso()) {
			auto qso = contest.popQso();
			std::cout << "\n[QSO] " << std::get<0>(qso)
			          << " " << std::get<1>(qso)
			          << " " << std::get<2>(qso)
				  << " (" << std::get<3>(qso) << "wpm)" << std::endl;
		}

		// Sleep 10ms
		usleep(10000);
	}

	std::cout << "\n\nShutting down...\n";
	contest.stop();

	std::cout << "\nFinal Statistics:\n";
	std::cout << "  Time: ";
	auto [h, m, s] = contest.time();
	std::cout << h << "h " << m << "m " << s << "s\n";
	std::cout << "  QSOs: " << contest.qsoQueue.size() << "\n";

	std::cout << "\nThank you for using Contest Simulator!\n";

	return 0;
}
