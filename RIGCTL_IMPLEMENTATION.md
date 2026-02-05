# Hamlib rigctld Implementation Summary

## Overview

Successfully implemented a hamlib-compatible rigctld network interface for the contest simulator. This allows contest logging software and other ham radio applications to control the simulator as if it were a real transceiver, with the primary use case being RIT (Receiver Incremental Tuning) control.

## Implementation Details

### Files Modified/Created

1. **winkeyer.h** - Added `RigctldServer` class declaration
2. **winkeyer.cpp** - Added `RigctldServer` implementation (~250 lines)
3. **testsim_daemon.cpp** - Integrated rigctld server with main loop
4. **RIGCTL_PROTOCOL.md** - Comprehensive protocol documentation
5. **test_rigctl.sh** - Test script for manual protocol verification
6. **README.md** - Updated to mention rigctld support
7. **CLAUDE.md** - Updated architecture documentation

### Key Components

#### RigctldServer Class (winkeyer.h/cpp)

```cpp
class RigctldServer {
public:
    RigctldServer(int port);
    void poll();  // Non-blocking command processing

    // Callbacks for state queries
    std::function<long long()> onGetFreq;
    std::function<std::string()> onGetMode;
    std::function<int()> onGetPassband;
    std::function<std::string()> onGetVfo;
    std::function<int()> onGetRit;

    // Callbacks for state changes
    std::function<void(long long)> onSetFreq;
    std::function<void(const std::string&, int)> onSetMode;
    std::function<void(const std::string&)> onSetVfo;
    std::function<void(int)> onSetRit;

    // Simulated radio state
    long long freq_hz;
    std::string mode;
    int passband_hz;
    std::string vfo;
};
```

**Design Pattern:**
- Follows same architecture as WinKeyerServer and CwdaemonServer
- Uses TcpTransport for network communication
- Non-blocking polling in main loop
- Callbacks connect to Contest class (RIT) or internal state (frequency, mode, VFO)

**Thread Safety:**
- RIT uses direct access to `contest.rit` (int is atomic)
- No mutex needed (same pattern as existing usage)
- Other state (freq, mode, VFO) maintained internally by RigctldServer

### Supported Commands

#### Priority 1 (Fully Implemented)

| Command | Short | Long | Description | State |
|---------|-------|------|-------------|-------|
| get_rit | `j` | `\get_rit` | Get RIT offset in Hz | ✅ Bidirectional with Contest |
| set_rit | `J 150` | `\set_rit 150` | Set RIT offset | ✅ Updates contest.rit |
| get_freq | `f` | `\get_freq` | Get VFO frequency | ✅ Internal to RigctldServer |
| set_freq | `F 14074000` | `\set_freq 14074000` | Set frequency | ✅ Internal to RigctldServer |
| get_mode | `m` | `\get_mode` | Get mode & passband | ✅ Internal to RigctldServer |
| set_mode | `M CW 500` | `\set_mode CW 500` | Set mode | ✅ Internal to RigctldServer |
| dump_state | - | `\dump_state` | Get capabilities | ✅ Minimal capability report |

#### Priority 2 (Implemented for Compatibility)

| Command | Short | Long | Description |
|---------|-------|------|-------------|
| get_vfo | `v` | `\get_vfo` | Get current VFO | ✅ |
| set_vfo | `V VFOA` | `\set_vfo VFOA` | Set VFO | ✅ |
| get_ptt | `t` | `\get_ptt` | Get PTT status | ✅ Returns 0 (RX) |
| set_ptt | `T 1` | `\set_ptt 1` | Set PTT | ✅ Ignored (auto) |
| get_status | `s` | - | Get rig status | ✅ Returns 0 |
| quit | `q` | - | Disconnect | ✅ Client disconnect |

### Protocol Features

**Response Format:**
- Get commands: value(s) on separate lines
- Set commands: `RPRT 0` (success) or `RPRT x` (error code)

**Error Codes:**
- `RIG_OK (0)` - Success
- `RIG_EINVAL (-1)` - Invalid parameter
- `RIG_ECONF (-2)` - Invalid configuration
- `RIG_ENIMPL (-4)` - Function not implemented
- `RIG_EIO (-5)` - I/O error
- `RIG_EPROTO (-10)` - Protocol error

**Command Format:**
- Short: Single character commands (`j`, `J 150`, `f`, etc.)
- Long: Backslash-prefixed (`\get_rit`, `\set_rit 150`, etc.)
- Newline-terminated command lines
- ASCII text protocol

### Integration with testsim_daemon.cpp

```cpp
// Command line argument
--rigctl-port PORT    Enable rigctld on specified port (default: disabled)

// Callback wiring
rigctl->onGetRit = [&contest]() -> int {
    return contest.rit;  // Direct read (thread-safe)
};

rigctl->onSetRit = [&contest](int rit_hz) {
    contest.rit = rit_hz;  // Direct write (applied in next audio buffer)
};

// Other callbacks return internal state
rigctl->onGetFreq = [&rigctl]() { return rigctl->freq_hz; };
rigctl->onSetFreq = [&rigctl](long long freq_hz) { rigctl->freq_hz = freq_hz; };
// etc.

// Main loop polling
while (running) {
    if (winkeyer) winkeyer->poll();
    if (cwdaemon) cwdaemon->poll();
    if (rigctl) rigctl->poll();  // Add rigctl polling
    // ... rest of loop
}
```

## Testing

### Manual Testing with telnet

```bash
$ ./testsim --rigctl-port 5555 --port 7891

$ telnet localhost 5555
j           # Get RIT -> 0
J 150       # Set RIT to +150 Hz -> RPRT 0
j           # Get RIT -> 150
f           # Get frequency -> 14074000
F 7074000   # Set frequency -> RPRT 0
f           # Get frequency -> 7074000
m           # Get mode -> CW / 500
M USB 2400  # Set mode -> RPRT 0
m           # Get mode -> USB / 2400
v           # Get VFO -> VFOA
V VFOB      # Set VFO -> RPRT 0
v           # Get VFO -> VFOB
\get_rit    # Long format -> 150
\set_rit 300 # Long format -> RPRT 0
\dump_state # Get capabilities -> (multi-line response)
q           # Quit -> RPRT 0
```

### Test Results

✅ All commands working correctly
✅ RIT changes persist across connections
✅ Frequency, mode, VFO state maintained correctly
✅ Short and long command formats both work
✅ Multiple connect/disconnect cycles successful
✅ Error handling for invalid commands
✅ dump_state returns proper capability information

### Test Script

Created `test_rigctl.sh` for automated testing:
- Tests all RIT commands (get/set with positive/negative values)
- Tests frequency control (get/set)
- Tests mode control (get/set)
- Tests VFO control (get/set)
- Tests long command format
- Tests status commands
- Tests dump_state

## Usage Examples

### Basic Usage

```bash
# Start simulator with rigctld enabled
./testsim --rigctl-port 4532 --port 7890

# In another terminal, test with telnet
telnet localhost 4532
j           # Get RIT (0)
J 150       # Set RIT to +150 Hz
q           # Quit
```

### With Contest Logger (N1MM+)

1. Configure radio:
   - Radio: Hamlib
   - Model: NET rigctl (model 2)
   - Address: localhost
   - Port: 4532

2. Use RIT controls in N1MM+ to adjust frequency
3. RIT changes will shift audio frequency in simulator

### With rigctl Command-Line Tool

```bash
# Test with hamlib rigctl
rigctl -m 2 -r localhost:4532

# Commands
\get_rit
\set_rit 100
\get_freq
```

### Typical Setup

```bash
# Full setup with all protocols
./testsim --port 7890 --rigctl-port 4532

# Configure logger:
# - WinKeyer on localhost:7890 for keying
# - rigctld on localhost:4532 for RIT control
```

## Architecture Decisions

### Why RIT Only is Bidirectional?

- **RIT** directly affects audio mixing (frequency shift via phase rotation)
- **Frequency/Mode/VFO** don't affect audio processing (cosmetic for logging software)
- Keeps implementation simple while providing real functionality

### Thread Safety

- `int` reads/writes are atomic on modern architectures
- `contest.rit` already accessed directly without locks in audio callback
- No mutex needed (consistent with existing design)

### Transport Reuse

- Uses existing `TcpTransport` class (same as WinKeyerServer)
- Follows established pattern for network servers
- Minimal code duplication

### Error Handling

- Standard hamlib error codes for compatibility
- Invalid commands return appropriate RPRT codes
- Protocol errors handled gracefully

## Future Enhancements (Not Implemented)

These could be added later if needed:

1. **Extended Response Protocol** - Verbose response format with +/;/| prefixes
2. **Multiple Clients** - Simultaneous connections with locking
3. **Frequency-Dependent Audio** - Frequency changes affect mixer
4. **Mode-Dependent Filtering** - Mode changes affect audio filtering
5. **Split Operation** - Separate TX/RX frequencies
6. **S-Meter** - Signal strength reporting based on audio levels
7. **Memory Channels** - Frequency/mode presets
8. **Band Stacking** - Per-band frequency memory

## Documentation

### Created Files

1. **RIGCTL_PROTOCOL.md** - Comprehensive protocol documentation
   - Overview and enabling instructions
   - Supported commands table
   - Response format
   - Configuration with various contest loggers
   - Implementation details
   - Troubleshooting guide
   - Example session

2. **test_rigctl.sh** - Manual test script
   - Tests all supported commands
   - Demonstrates protocol usage
   - Useful for verification after changes

### Updated Files

1. **README.md**
   - Added rigctld to logger integration section
   - Updated quick start examples
   - Added documentation links

2. **CLAUDE.md**
   - Updated usage section
   - Updated network keyer interfaces documentation
   - Added rigctld protocol details

## Success Criteria

✅ **Basic connectivity** - telnet works, commands execute
✅ **RIT control** - Set RIT via `J` command, value persists
✅ **State queries** - Get frequency, mode, VFO return correct values
✅ **Error handling** - Invalid commands return proper RPRT codes
✅ **Multiple connects** - Server handles disconnect/reconnect gracefully
✅ **Protocol compliance** - Follows hamlib rigctld conventions
✅ **Documentation** - Complete user and developer docs
✅ **Testing** - Manual test script and verification

## Conclusion

The rigctld implementation is **complete and production-ready**. It follows the existing architecture patterns, integrates cleanly with the Contest class, provides useful RIT control functionality, and maintains compatibility with hamlib-based tools and contest logging software.

Key benefits:
- Contest loggers can adjust RIT to shift audio frequency
- Maintains state across connections
- Works alongside WinKeyer/cwdaemon protocols
- Minimal code (~250 lines)
- No thread safety issues
- Comprehensive documentation
