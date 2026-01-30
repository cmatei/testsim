#include "winkeyer.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// WinKeyer3 command definitions
#define WK_ADMIN           0x00
#define WK_SIDETONE        0x01
#define WK_WPM_SPEED       0x02
#define WK_WEIGHT          0x03
#define WK_PTT             0x04
#define WK_POTSETUP        0x05
#define WK_PAUSE           0x07
#define WK_BACKSPACE       0x08
#define WK_KEY_IMMEDIATE   0x09
#define WK_HSCW_SPEED      0x0A
#define WK_BUFFERED_SPEED  0x0B
#define WK_BUFFERED_PTT    0x0C
#define WK_KEY_COMP        0x0D
#define WK_PADDLE_SWITCH   0x0E
#define WK_NULL_CMD        0x0F
#define WK_SOFTWARE_PADDLE 0x10
#define WK_STATUS_REQUEST  0x11
#define WK_POINTER_CMD     0x12
#define WK_BACKSPACE_CMD   0x13
#define WK_CLEAR_BUFFER    0x15
#define WK_KEY_BUFFERED    0x17
#define WK_BUFFERED_WAIT   0x18
#define WK_POINTER_0       0x19
#define WK_POINTER_1       0x1A
#define WK_POINTER_2       0x1B
#define WK_POINTER_3       0x1C
#define WK_CANCEL_SPEED    0x1D
#define WK_BUFFERED_NOP    0x1E
#define WK_ADMIN_EXT       0x1F

// ===== TcpTransport Implementation =====

TcpTransport::TcpTransport(int port)
	: listen_fd(-1), client_fd(-1), port(port)
{
	if (!createListener(port)) {
		std::cerr << "Warning: Could not create TCP listener on port " << port << std::endl;
	}
}

TcpTransport::~TcpTransport()
{
	close();
}

bool TcpTransport::createListener(int port)
{
	listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_fd < 0) {
		std::cerr << "Error: Failed to create socket: " << strerror(errno) << std::endl;
		return false;
	}

	// Set socket options
	int opt = 1;
	if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
		std::cerr << "Error: setsockopt SO_REUSEADDR failed: " << strerror(errno) << std::endl;
		::close(listen_fd);
		listen_fd = -1;
		return false;
	}

	// Set non-blocking
	int flags = fcntl(listen_fd, F_GETFL, 0);
	fcntl(listen_fd, F_SETFL, flags | O_NONBLOCK);

	// Bind to port
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(port);

	if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
		std::cerr << "Error: Failed to bind to port " << port << ": " << strerror(errno) << std::endl;
		::close(listen_fd);
		listen_fd = -1;
		return false;
	}

	// Listen
	if (listen(listen_fd, 1) < 0) {
		std::cerr << "Error: Failed to listen on port " << port << ": " << strerror(errno) << std::endl;
		::close(listen_fd);
		listen_fd = -1;
		return false;
	}

	std::cout << "WinKeyer: TCP server listening on port " << port << std::endl;
	std::cout << "Configure your logger to connect to: localhost:" << port << std::endl;
	return true;
}

void TcpTransport::pollConnections()
{
	if (listen_fd < 0) return;

	// Check for new connections
	struct sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);

	int new_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &client_len);
	if (new_fd >= 0) {
		// Close existing client if any
		if (client_fd >= 0) {
			std::cout << "WinKeyer: Closing existing client connection" << std::endl;
			closeClient();
		}

		// Set non-blocking
		int flags = fcntl(new_fd, F_GETFL, 0);
		fcntl(new_fd, F_SETFL, flags | O_NONBLOCK);

		client_fd = new_fd;
		char client_ip[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
		std::cout << "WinKeyer: Client connected from " << client_ip
		          << ":" << ntohs(client_addr.sin_port) << std::endl;
	}
}

void TcpTransport::closeClient()
{
	if (client_fd >= 0) {
		::close(client_fd);
		client_fd = -1;
		std::cout << "WinKeyer: Client disconnected" << std::endl;
	}
}

void TcpTransport::close()
{
	closeClient();
	if (listen_fd >= 0) {
		::close(listen_fd);
		listen_fd = -1;
	}
}

int TcpTransport::readByte()
{
	if (client_fd < 0) return -1;

	unsigned char byte;
	ssize_t n = recv(client_fd, &byte, 1, 0);
	if (n == 1) {
		return byte;
	} else if (n == 0) {
		// Client disconnected
		closeClient();
	} else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
		// Error (not just "no data available")
		std::cerr << "WinKeyer: Read error: " << strerror(errno) << std::endl;
		closeClient();
	}
	return -1;
}

void TcpTransport::writeByte(unsigned char byte)
{
	if (client_fd >= 0) {
		send(client_fd, &byte, 1, 0);
	}
}

void TcpTransport::writeBytes(const unsigned char *data, size_t len)
{
	if (client_fd >= 0) {
		send(client_fd, data, len, 0);
	}
}

// ===== WinKeyerServer Implementation =====

WinKeyerServer::WinKeyerServer(int port, int version)
	: protocol_version(version), current_wpm(30), busy(false), breakin(false), wait_flag(false),
	  initialized(false), expected_bytes(0), in_text_mode(false)
{
	transport = std::make_unique<TcpTransport>(port);
}

WinKeyerServer::~WinKeyerServer()
{
}

unsigned char WinKeyerServer::getStatusByte() const
{
	unsigned char status = 0;
	if (breakin) status |= 0x01;
	if (busy) status |= 0x02;
	if (wait_flag) status |= 0x04;
	// Bit 4: XOFF state (not implemented)
	// Bit 6: Breakin pending (not implemented)
	return status;
}

void WinKeyerServer::sendStatus()
{
	unsigned char status = getStatusByte();
	transport->writeByte(status);
}

void WinKeyerServer::setBusy(bool b)
{
	busy = b;
	// Send status update if logger is waiting
	if (initialized && transport->isOpen()) {
		sendStatus();
	}
}

void WinKeyerServer::setBreakIn(bool b)
{
	breakin = b;
}

void WinKeyerServer::processCommand(unsigned char cmd)
{
	switch (cmd) {
		case WK_ADMIN: {
			// Admin command - usually 0x00 to reset/initialize
			// Wait for second byte
			int param = transport->readByte();
			if (param >= 0) {
				if (param == 0x02) {
					// Open command - initialize
					initialized = true;
					std::cout << "WinKeyer" << protocol_version << ": Initialized" << std::endl;
					// Send version info (version 23 = WK2, version 31 = WK3)
					int version_byte = (protocol_version == 2) ? 23 : 31;
					transport->writeByte(version_byte);
				} else if (param == 0x03) {
					// Close command
					initialized = false;
					std::cout << "WinKeyer" << protocol_version << ": Closed" << std::endl;
				}
				sendStatus();
			}
			break;
		}

		case WK_SIDETONE: {
			// Sidetone frequency (0-9, or 1-4000 Hz in extended mode)
			int freq = transport->readByte();
			// Ignore for simulator
			break;
		}

		case WK_WPM_SPEED: {
			// Set WPM speed
			int wpm = transport->readByte();
			if (wpm >= 0) {
				current_wpm = wpm;
				if (onSpeedChange) {
					onSpeedChange(wpm);
				}
				std::cout << "WinKeyer: Speed set to " << wpm << " WPM" << std::endl;
			}
			break;
		}

		case WK_WEIGHT: {
			// Dit/dah ratio weight (ignored for simulator)
			int weight = transport->readByte();
			break;
		}

		case WK_PTT: {
			// PTT on/off
			int state = transport->readByte();
			if (state >= 0) {
				bool ptt_on = (state & 0x01) != 0;
				if (onPttChange) {
					onPttChange(ptt_on);
				}
			}
			break;
		}

		case WK_BUFFERED_SPEED: {
			// Speed change in buffer
			int wpm = transport->readByte();
			if (wpm >= 0) {
				current_wpm = wpm;
				if (onSpeedChange) {
					onSpeedChange(wpm);
				}
			}
			break;
		}

		case WK_BUFFERED_PTT: {
			// PTT in buffer
			int state = transport->readByte();
			if (state >= 0) {
				bool ptt_on = (state & 0x01) != 0;
				if (onPttChange) {
					onPttChange(ptt_on);
				}
			}
			break;
		}

		case WK_NULL_CMD:
		case WK_STATUS_REQUEST: {
			// Status request - send current status
			sendStatus();
			break;
		}

		case WK_CLEAR_BUFFER: {
			// Clear the keyer buffer
			text_buffer.clear();
			in_text_mode = false;
			busy = false;
			sendStatus();
			break;
		}

		case WK_KEY_IMMEDIATE: {
			// Key down immediately (not buffered)
			// For simulator, we can ignore this
			break;
		}

		case WK_BUFFERED_NOP: {
			// NOP in buffer
			break;
		}

		case WK_PAUSE: {
			// Pause/resume keying
			int state = transport->readByte();
			// For simulator, we could pause MyStation
			break;
		}

		default: {
			// Unknown command or text data
			// Check if it's printable ASCII (text message)
			if (cmd >= 0x20 && cmd <= 0x7E) {
				// Text character
				text_buffer += static_cast<char>(cmd);
				in_text_mode = true;
			} else if (cmd == 0x00 && in_text_mode) {
				// Null terminator - end of text message
				if (!text_buffer.empty() && onTextToSend) {
					std::cout << "WinKeyer: Send text: " << text_buffer << std::endl;
					onTextToSend(text_buffer);
					text_buffer.clear();
				}
				in_text_mode = false;
			}
			break;
		}
	}
}

void WinKeyerServer::poll()
{
	if (!transport || !transport->isOpen()) return;

	// Poll for new connections (TCP only)
	transport->pollConnections();

	// Read available bytes
	int byte;
	while ((byte = transport->readByte()) >= 0) {
		processCommand(static_cast<unsigned char>(byte));
	}
}
