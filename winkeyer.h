#ifndef __WINKEYER_H
#define __WINKEYER_H

#include <string>
#include <functional>
#include <vector>
#include <queue>
#include <memory>
#include <netinet/in.h>

/**
 * Transport interface - abstraction for network communication
 */
class Transport {
public:
	virtual ~Transport() = default;

	// Read a single byte (-1 if none available)
	virtual int readByte() = 0;

	// Write a single byte
	virtual void writeByte(unsigned char byte) = 0;

	// Write multiple bytes
	virtual void writeBytes(const unsigned char *data, size_t len) = 0;

	// Check if transport is open/connected
	virtual bool isOpen() const = 0;

	// Close the transport
	virtual void close() = 0;

	// Poll for new connections
	virtual void pollConnections() {}
};

/**
 * TCP socket transport
 */
class TcpTransport : public Transport {
public:
	TcpTransport(int port);
	~TcpTransport() override;

	int readByte() override;
	void writeByte(unsigned char byte) override;
	void writeBytes(const unsigned char *data, size_t len) override;
	bool isOpen() const override { return listen_fd >= 0; }
	void close() override;
	void pollConnections() override;

	bool hasClient() const { return client_fd >= 0; }

private:
	bool createListener(int port);
	void closeClient();

	int listen_fd;   // Listening socket
	int client_fd;   // Connected client
	int port;
};

/**
 * UDP socket transport for cwdaemon
 */
class UdpTransport : public Transport {
public:
	UdpTransport(int port);
	~UdpTransport() override;

	int readByte() override;
	void writeByte(unsigned char byte) override;
	void writeBytes(const unsigned char *data, size_t len) override;
	bool isOpen() const override { return socket_fd >= 0; }
	void close() override;

	// Read full datagram (cwdaemon messages are complete datagrams)
	int readDatagram(unsigned char *buffer, size_t max_len);

private:
	bool createSocket(int port);

	int socket_fd;
	int port;
	struct sockaddr_in client_addr;  // Last client address for replies
	socklen_t client_addr_len;
	bool has_client;  // Track if we have a client to reply to

	// Internal buffer for byte-by-byte reading
	std::vector<unsigned char> read_buffer;
	size_t read_pos;
};

/**
 * WinKeyerServer - Implements WinKeyer2/3 protocol over TCP socket
 *
 * This class handles the WinKeyer2 or WinKeyer3 protocol, allowing contest logging
 * software (N1MM+, Win-Test, etc.) to control the CW keyer over the network.
 *
 * The logger sends keying commands, and this class decodes them into
 * text messages that can be sent via MyStation.
 */
class WinKeyerServer {
public:
	// Create WinKeyer server on specified TCP port with protocol version (2 or 3)
	WinKeyerServer(int port, int version = 3);

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
	bool isOpen() const { return transport && transport->isOpen(); }

private:
	// Protocol handling
	void processCommand(unsigned char cmd);
	void sendStatus();

	// Status tracking
	unsigned char getStatusByte() const;

	std::unique_ptr<Transport> transport;

	// WinKeyer state
	int protocol_version;  // 2 or 3
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

/**
 * CwdaemonServer - Implements cwdaemon protocol over UDP socket
 *
 * This class handles the cwdaemon protocol, an alternative to WinKeyer used by
 * many Linux contest loggers (TLF, xlog, etc.). Uses UDP instead of TCP and
 * an ESC-based command protocol.
 */
class CwdaemonServer {
public:
	CwdaemonServer(int port);
	~CwdaemonServer();

	// Poll for incoming datagrams
	void poll();

	// Callbacks (same as WinKeyerServer)
	std::function<void(const std::string&)> onTextToSend;
	std::function<void(const std::string&)> onDetectMessage;
	std::function<void(bool)> onPttChange;
	std::function<void(int)> onSpeedChange;
	std::function<void()> onAbort;

	// Get current state
	int getWpm() const { return current_wpm; }
	bool isOpen() const { return transport && transport->isOpen(); }

	// Set reply text (for ESC-h command)
	void setReply(const std::string& reply);

private:
	void processMessage(const unsigned char *data, size_t len);
	void processEscCommand(unsigned char cmd, const unsigned char *data, size_t len, size_t &pos);
	void processText(const std::string &text);
	void sendReply();

	std::unique_ptr<UdpTransport> transport;
	int current_wpm;
	int base_wpm;      // WPM before inline speed changes, restored on abort
	int current_tone;
	int current_weight;
	bool ptt_state;
	std::string reply_text;  // For ESC-h command

	static constexpr unsigned char ESC = 0x1B;
};

#endif
