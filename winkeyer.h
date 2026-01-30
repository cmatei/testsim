#ifndef __WINKEYER_H
#define __WINKEYER_H

#include <string>
#include <functional>
#include <vector>
#include <queue>

/**
 * WinKeyerServer - Implements WinKeyer3 protocol over serial port
 *
 * This class handles the WinKeyer3 USB protocol, allowing contest logging
 * software (N1MM+, Win-Test, etc.) to control the CW keyer.
 *
 * The logger sends keying commands, and this class decodes them into
 * text messages that can be sent via MyStation.
 */
class WinKeyerServer {
public:
	WinKeyerServer(const std::string &device);
	~WinKeyerServer();

	// Poll for incoming data (call regularly from main loop)
	void poll();

	// Update status sent back to logger
	void setBusy(bool busy);
	void setBreakIn(bool breakin);

	// Callbacks for decoded messages
	std::function<void(const std::string&)> onTextToSend;
	std::function<void(bool)> onPttChange;
	std::function<void(int)> onSpeedChange;

	// Get current WPM from logger
	int getWpm() const { return current_wpm; }

	// Check if connected
	bool isOpen() const { return fd >= 0; }

private:
	// Serial port handling
	bool openPort(const std::string &device);
	void closePort();
	int readByte();
	void writeByte(unsigned char byte);
	void writeBytes(const unsigned char *data, size_t len);

	// Protocol handling
	void processCommand(unsigned char cmd);
	void sendStatus();

	// Status tracking
	unsigned char getStatusByte() const;

	int fd;  // File descriptor for serial port
	std::string device_path;

	// WinKeyer state
	int current_wpm;
	bool busy;
	bool breakin;
	bool wait_flag;
	bool initialized;

	// Command buffer for multi-byte commands
	std::vector<unsigned char> cmd_buffer;
	int expected_bytes;

	// Text buffer for 0x1B command
	std::string text_buffer;
	bool in_text_mode;
};

#endif
