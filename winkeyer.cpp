#include "winkeyer.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>

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

WinKeyerServer::WinKeyerServer(const std::string &device)
	: fd(-1), device_path(device), current_wpm(30),
	  busy(false), breakin(false), wait_flag(false),
	  initialized(false), expected_bytes(0), in_text_mode(false)
{
	if (!openPort(device)) {
		std::cerr << "Warning: Could not open WinKeyer device: " << device << std::endl;
		std::cerr << "To create a virtual serial port pair, use:" << std::endl;
		std::cerr << "  socat -d -d pty,raw,echo=0 pty,raw,echo=0" << std::endl;
		std::cerr << "Then connect your logger to one end and testsim to the other." << std::endl;
	}
}

WinKeyerServer::~WinKeyerServer()
{
	closePort();
}

bool WinKeyerServer::openPort(const std::string &device)
{
	fd = open(device.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (fd < 0) {
		return false;
	}

	struct termios tty;
	memset(&tty, 0, sizeof(tty));

	if (tcgetattr(fd, &tty) != 0) {
		close(fd);
		fd = -1;
		return false;
	}

	// WinKeyer3 USB: 1200 baud, 8N1
	cfsetospeed(&tty, B1200);
	cfsetispeed(&tty, B1200);

	tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;  // 8 bits
	tty.c_iflag &= ~IGNBRK;  // Disable break processing
	tty.c_lflag = 0;         // No signaling chars, no echo, no canonical
	tty.c_oflag = 0;         // No remapping, no delays
	tty.c_cc[VMIN] = 0;      // Non-blocking
	tty.c_cc[VTIME] = 0;

	tty.c_iflag &= ~(IXON | IXOFF | IXANY);  // No flow control
	tty.c_cflag |= (CLOCAL | CREAD);         // Local, enable receiver
	tty.c_cflag &= ~(PARENB | PARODD);       // No parity
	tty.c_cflag &= ~CSTOPB;                  // 1 stop bit

	if (tcsetattr(fd, TCSANOW, &tty) != 0) {
		close(fd);
		fd = -1;
		return false;
	}

	std::cout << "WinKeyer3: Opened " << device << std::endl;
	return true;
}

void WinKeyerServer::closePort()
{
	if (fd >= 0) {
		close(fd);
		fd = -1;
	}
}

int WinKeyerServer::readByte()
{
	unsigned char byte;
	ssize_t n = read(fd, &byte, 1);
	if (n == 1) {
		return byte;
	}
	return -1;
}

void WinKeyerServer::writeByte(unsigned char byte)
{
	write(fd, &byte, 1);
}

void WinKeyerServer::writeBytes(const unsigned char *data, size_t len)
{
	write(fd, data, len);
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
	writeByte(status);
}

void WinKeyerServer::setBusy(bool b)
{
	busy = b;
	// Send status update if logger is waiting
	if (initialized) {
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
			int param = readByte();
			if (param >= 0) {
				if (param == 0x02) {
					// Open command - initialize
					initialized = true;
					std::cout << "WinKeyer3: Initialized" << std::endl;
					// Send version info (version 31 = WK3)
					writeByte(31);
				} else if (param == 0x03) {
					// Close command
					initialized = false;
					std::cout << "WinKeyer3: Closed" << std::endl;
				}
				sendStatus();
			}
			break;
		}

		case WK_SIDETONE: {
			// Sidetone frequency (0-9, or 1-4000 Hz in extended mode)
			int freq = readByte();
			// Ignore for simulator
			break;
		}

		case WK_WPM_SPEED: {
			// Set WPM speed
			int wpm = readByte();
			if (wpm >= 0) {
				current_wpm = wpm;
				if (onSpeedChange) {
					onSpeedChange(wpm);
				}
				std::cout << "WinKeyer3: Speed set to " << wpm << " WPM" << std::endl;
			}
			break;
		}

		case WK_WEIGHT: {
			// Dit/dah ratio weight (ignored for simulator)
			int weight = readByte();
			break;
		}

		case WK_PTT: {
			// PTT on/off
			int state = readByte();
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
			int wpm = readByte();
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
			int state = readByte();
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
			int state = readByte();
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
					std::cout << "WinKeyer3: Send text: " << text_buffer << std::endl;
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
	if (fd < 0) return;

	// Read available bytes
	int byte;
	while ((byte = readByte()) >= 0) {
		processCommand(static_cast<unsigned char>(byte));
	}
}
