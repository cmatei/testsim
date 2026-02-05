# Hamlib rigctld Protocol Support

The contest simulator implements a hamlib-compatible rigctld network interface, allowing contest logging software and other ham radio applications to control the simulator as if it were a real transceiver.

## Overview

The rigctld server provides a TCP-based interface using the hamlib protocol. The primary use case is **RIT (Receiver Incremental Tuning) control**, but a minimal set of transceiver features are implemented for realism and compatibility with logging software.

## Enabling rigctld

Start the simulator with the `--rigctl-port` option:

```bash
# Enable rigctld on default hamlib port
./testsim --rigctl-port 4532 --winkeyer-port 7890

# Use a different port
./testsim --rigctl-port 5555 --winkeyer-port 7890
```

The rigctld server runs independently of the WinKeyer/cwdaemon servers and can be used alongside them.

## Supported Commands

### RIT Control (Primary Feature)

| Command | Syntax | Response | Description |
|---------|--------|----------|-------------|
| `j` or `\get_rit` | `j\n` | `150\n` | Get RIT offset in Hz |
| `J` or `\set_rit` | `J 150\n` | `RPRT 0\n` | Set RIT offset in Hz |

**RIT implementation:**
- Value in Hz (integer)
- Applied immediately to audio mixing (frequency shift)
- Typical range: ±500 to ±5000 Hz (no enforced limits)
- Thread-safe: Direct access to `contest.rit`

**Example:**
```bash
# Set RIT to +150 Hz (stations will sound 150 Hz higher)
J 150

# Get current RIT
j
# Response: 150
```

### Frequency Control

| Command | Syntax | Response | Description |
|---------|--------|----------|-------------|
| `f` or `\get_freq` | `f\n` | `14074000\n` | Get VFO frequency in Hz |
| `F` or `\set_freq` | `F 14074000\n` | `RPRT 0\n` | Set VFO frequency in Hz |

**Notes:**
- Default frequency: 14074000 Hz (20m CW)
- Cosmetic only (doesn't affect audio mixing)
- State maintained by rigctld server

### Mode Control

| Command | Syntax | Response | Description |
|---------|--------|----------|-------------|
| `m` or `\get_mode` | `m\n` | `CW\n500\n` | Get mode and passband |
| `M` or `\set_mode` | `M CW 500\n` | `RPRT 0\n` | Set mode and passband |

**Supported modes:**
- `CW` (default)
- `USB`
- `LSB`
- `AM`
- `FM`

**Notes:**
- Default: CW, 500 Hz passband
- Cosmetic only (doesn't affect audio filtering)
- State maintained by rigctld server

### VFO Control

| Command | Syntax | Response | Description |
|---------|--------|----------|-------------|
| `v` or `\get_vfo` | `v\n` | `VFOA\n` | Get current VFO |
| `V` or `\set_vfo` | `V VFOA\n` | `RPRT 0\n` | Set VFO |

**Supported VFOs:**
- `VFOA` (default)
- `VFOB`

**Notes:**
- Cosmetic only (simulator has single receiver)
- State maintained by rigctld server

### Status Commands

| Command | Syntax | Response | Description |
|---------|--------|----------|-------------|
| `t` or `\get_ptt` | `t\n` | `0\n` | Get PTT status (always RX) |
| `T` or `\set_ptt` | `T 1\n` | `RPRT 0\n` | Set PTT (ignored) |
| `s` or `\get_status` | `s\n` | `0\n` | Get rig status |
| `\dump_state` | `\dump_state\n` | (capabilities) | Get rig capabilities |
| `q` | `q\n` | `RPRT 0\n` | Quit/disconnect |

**Notes:**
- PTT is handled automatically by MyStation transmission state
- `dump_state` returns minimal capabilities for compatibility

## Response Format

**Get commands** return values on separate lines:
```
f
14074000

m
CW
500
```

**Set commands** return success/error codes:
```
RPRT 0    # Success
RPRT -1   # Invalid parameter
RPRT -4   # Not implemented
RPRT -10  # Protocol error
```

## Configuration with Contest Loggers

### N1MM+

1. Configure Rig:
   - Radio: **Hamlib**
   - Model: **NET rigctl (model 2)**
   - Address: **localhost**
   - Port: **4532** (or your --rigctl-port)
   - Baud: (ignored for network)

2. Enable RIT control in N1MM+ settings

### Win-Test

1. Configure Radio:
   - Type: **Hamlib**
   - Model: **NET rigctl**
   - Connection: **Network**
   - Address: **localhost:4532**

2. Use RIT controls in logging window

### rigctl Command Line

Test with the hamlib command-line tool:

```bash
# Interactive mode
rigctl -m 2 -r localhost:4532

# Single command
rigctl -m 2 -r localhost:4532 j     # Get RIT
rigctl -m 2 -r localhost:4532 J 100 # Set RIT to +100 Hz
```

### Telnet Testing

For manual testing and debugging:

```bash
telnet localhost 4532

# Try these commands:
j           # Get RIT
J 150       # Set RIT to +150 Hz
f           # Get frequency
F 7074000   # Set frequency
m           # Get mode
M USB 2400  # Set mode
\dump_state # Get capabilities
q           # Quit
```

## Implementation Details

### Architecture

The rigctld implementation follows the same pattern as WinKeyer and cwdaemon servers:

1. **Transport Layer**: Uses `TcpTransport` for network communication
2. **Protocol Layer**: `RigctldServer` class parses commands and generates responses
3. **State Management**: Callbacks connect to Contest class (RIT) or internal state
4. **Main Loop**: Non-blocking polling in main thread

### RIT Integration

RIT is the only bidirectional state between rigctld and Contest:

```cpp
// Get RIT (read from Contest)
rigctl->onGetRit = [&contest]() -> int {
    return contest.rit;
};

// Set RIT (write to Contest)
rigctl->onSetRit = [&contest](int rit_hz) {
    contest.rit = rit_hz;
};
```

**Thread safety:**
- `int` reads/writes are atomic on modern architectures
- No mutex needed (same as existing `contest.rit` usage)
- Value applied in next audio buffer

### Other State

Frequency, mode, VFO state is maintained internally by `RigctldServer`:

```cpp
// Simulated radio state
long long freq_hz;    // Default: 14074000 (20m CW)
std::string mode;     // Default: "CW"
int passband_hz;      // Default: 500
std::string vfo;      // Default: "VFOA"
```

These are cosmetic and don't affect audio processing.

### Command Protocol

The server supports both short (single character) and long (backslash-prefixed) command formats:

```
# Short format
j           # get_rit
J 150       # set_rit

# Long format
\get_rit
\set_rit 150
```

### Error Handling

Standard hamlib error codes:

| Code | Constant | Meaning |
|------|----------|---------|
| 0 | `RIG_OK` | Success |
| -1 | `RIG_EINVAL` | Invalid parameter |
| -2 | `RIG_ECONF` | Invalid configuration |
| -4 | `RIG_ENIMPL` | Function not implemented |
| -5 | `RIG_EIO` | I/O error |
| -10 | `RIG_EPROTO` | Protocol error |

## Troubleshooting

### Connection Issues

**Problem:** Logger can't connect to rigctld
- Check port is not in use: `netstat -an | grep 4532`
- Verify simulator is running: look for "rigctld: TCP server listening"
- Try telnet: `telnet localhost 4532`

**Problem:** Multiple connects fail
- rigctld accepts one client at a time
- Close existing connection before reconnecting
- Check logger's reconnection settings

### RIT Not Working

**Problem:** RIT changes don't affect audio
- Verify RIT is set: `telnet localhost 4532`, then `j`
- Check audio is playing (other stations audible)
- RIT range is typically ±500 to ±5000 Hz

**Problem:** RIT value doesn't persist
- Each `J` command sets absolute value (not relative)
- Logger may reset RIT on mode/frequency changes
- Check logger's RIT behavior settings

### Protocol Errors

**Problem:** Commands return `RPRT -1` or `RPRT -10`
- Check command format (newline-terminated)
- Verify parameter types (integers for freq/RIT)
- Use telnet to test command syntax

## Example Session

```bash
$ ./testsim --rigctl-port 4532 --winkeyer-port 7890
=====================================
      Contest Simulator
=====================================

Initializing contest simulator...
Configuration:
  Call:      YO3CEM
  WPM:       30
  Pitch:     500 Hz
  Bandwidth: 500 Hz
  ...

Starting WinKeyer3 TCP server on port 7890
WinKeyer: TCP server listening on port 7890
Configure your logger to connect to: localhost:7890

Starting rigctld TCP server on port 4532
rigctld: TCP server listening on port 4532
Configure your logger for hamlib rig model 2 at: localhost:4532

Starting audio output...

=== Contest Running ===
WinKeyer3 listening on: localhost:7890
rigctld listening on: localhost:4532
Press Ctrl+C to stop

[00:00:05] DX: 3 WPM: 30 RX
```

In another terminal:
```bash
$ telnet localhost 4532
Trying 127.0.0.1...
Connected to localhost.

j
0

J 150
RPRT 0

j
150

f
14074000

F 7074000
RPRT 0

m
CW
500

M USB 2400
RPRT 0

q
RPRT 0
Connection closed by foreign host.
```

## Future Enhancements

Potential future improvements (not currently implemented):

1. **Extended Response Protocol** - Verbose response format with +/;/| prefixes
2. **Multiple Clients** - Simultaneous connections with locking
3. **Frequency-Dependent Audio** - Frequency changes affect mixer
4. **Mode-Dependent Filtering** - Mode changes affect audio filtering
5. **Split Operation** - Separate TX/RX frequencies
6. **S-Meter** - Signal strength reporting based on audio levels
7. **Memory Channels** - Frequency/mode presets
8. **Band Stacking** - Per-band frequency memory

## References

- [Hamlib Network Device Control](https://github.com/Hamlib/Hamlib/wiki/Network-Device-Control)
- [rigctld Manual](https://hamlib.sourceforge.net/html/rigctld.1.html)
- [Hamlib Protocol Documentation](https://github.com/Hamlib/Hamlib/wiki)
