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
	std::cout << "Contest Simulator - WinKeyer3 Network Mode\n";
	std::cout << "Usage: " << prog << " [options]\n\n";
	std::cout << "Options:\n";
	std::cout << "  --port <port>        TCP port for WinKeyer3 (default: 7890)\n";
	std::cout << "  --config <file>      Configuration file (default: none, uses defaults)\n";
	std::cout << "  --help               Show this help\n\n";
	std::cout << "Network Mode:\n";
	std::cout << "  Contest Simulator runs a WinKeyer3 TCP server.\n";
	std::cout << "  Configure your logger to connect to: localhost:<port>\n";
	std::cout << "  Some loggers may call this 'Network WinKeyer' or 'TCP WinKeyer'\n\n";
	std::cout << "Examples:\n";
	std::cout << "  " << prog << " --port 7890\n";
	std::cout << "  " << prog << " --port 7890 --config contest.ini\n\n";
	std::cout << "Audio Routing:\n";
	std::cout << "  Configure your logger to use 'pulse' or system default audio input\n";
	std::cout << "  The simulator will output to the default audio device\n\n";
}

void print_status(Contest *contest, WinKeyerServer *wk)
{
	auto [h, m, s] = contest->time();
	int qsos = contest->qsoQueue.size();
	int dx_count = contest->dxCount();

	std::cout << "\r["
	          << (h < 10 ? "0" : "") << h << ":"
	          << (m < 10 ? "0" : "") << m << ":"
	          << (s < 10 ? "0" : "") << s << "] "
	          << "QSOs: " << qsos << " "
	          << "DX: " << dx_count << " "
	          << "WPM: " << wk->getWpm() << " "
	          << (contest->me->state == station_state::sending ? "TX" : "RX")
	          << "          " << std::flush;
}

int main(int argc, char **argv)
{
	int tcp_port = 7890;  // Default port
	std::string config_file;

	// Parse command line arguments
	for (int i = 1; i < argc; i++) {
		std::string arg = argv[i];
		if (arg == "--help" || arg == "-h") {
			print_usage(argv[0]);
			return 0;
		} else if (arg == "--port" && i + 1 < argc) {
			tcp_port = std::stoi(argv[++i]);
		} else if (arg == "--config" && i + 1 < argc) {
			config_file = argv[++i];
		} else {
			std::cerr << "Unknown option: " << arg << std::endl;
			print_usage(argv[0]);
			return 1;
		}
	}

	std::cout << "=====================================\n";
	std::cout << "   Contest Simulator (WinKeyer3)    \n";
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

	// Create WinKeyer server
	std::cout << "Starting WinKeyer3 TCP server on port " << tcp_port << "\n";
	auto winkeyer = std::make_unique<WinKeyerServer>(tcp_port);

	if (!winkeyer->isOpen()) {
		std::cerr << "\nError: Could not create TCP listener on port " << tcp_port << "\n";
		std::cerr << "Make sure the port is not already in use.\n";
		return 1;
	}

	// Connect WinKeyer to Contest/MyStation
	winkeyer->onTextToSend = [&contest](const std::string &text) {
		// Convert to uppercase for CW
		std::string upper = text;
		for (auto &c : upper) {
			c = std::toupper(c);
		}
		contest.me->sendText(upper);
	};

	winkeyer->onSpeedChange = [&contest](int wpm) {
		contest.setWpm(wpm);
	};

	winkeyer->onPttChange = [](bool ptt) {
		// PTT changes handled automatically by MyStation state
	};

	// Update WinKeyer busy status based on MyStation state
	auto last_state = contest.me->state;

	// Start audio
	std::cout << "Starting audio output...\n";
	contest.start();

	std::cout << "\n=== Contest Running ===\n";
	std::cout << "Waiting for logger to connect to: localhost:" << tcp_port << "\n";
	std::cout << "Press Ctrl+C to stop\n\n";

	// Main loop
	int status_counter = 0;
	while (g_running) {
		// Poll WinKeyer for incoming commands
		winkeyer->poll();

		// Update busy status if MyStation state changed
		if (contest.me->state != last_state) {
			bool is_sending = (contest.me->state == station_state::sending);
			winkeyer->setBusy(is_sending);
			last_state = contest.me->state;
		}

		// Print status every ~100ms
		if (++status_counter >= 10) {
			print_status(&contest, winkeyer.get());
			status_counter = 0;
		}

		// Check for completed QSOs
		while (!contest.qsoQueue.empty()) {
			auto qso = contest.qsoQueue.front();
			contest.qsoQueue.pop();
			std::cout << "\n[QSO] " << std::get<0>(qso)
			          << " " << std::get<1>(qso)
			          << " " << std::get<2>(qso) << std::endl;
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
