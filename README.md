# Contest Simulator - CW Contest Simulator for Training

A realistic CW (Morse code) contest simulator that integrates with popular contest logging software via cwdaemon protocol.

## What is Contest Simulator?

Contest Simulator simulates a complete contest environment with:
- **Realistic pile-ups** with multiple calling stations
- **Signal propagation effects**: QSB (fading), QRN (atmospheric noise), QRM (interference)
- **Operator behavior simulation**: Mistakes, impatience, lid behavior
- **Logger integration**: Works with TLF, xlog, CQRLog, and other cwdaemon-compatible loggers

Instead of a built-in GUI, Contest Simulator acts as a **virtual contest radio** that your logging software controls via network CW keyer protocol, providing a more realistic practice environment.

## Features

### Realistic Simulation
- Multiple DX stations call simultaneously (pile-up mode)
- Signal fading (QSB) with Gaussian process modeling
- Atmospheric noise bursts (QRN)
- Station interference (QRM)
- Operator mistakes and varying skill levels
- Variable sending speeds and timing

### Logger Integration
- **WinKeyer2/3** protocol (TCP) - N1MM+, Win-Test, DXLog.net, TRX-Manager
- **cwdaemon** protocol (UDP) - TLF, xlog, CQRLog, Tucnak
- **rigctld** protocol (TCP) - Hamlib-compatible rig control (RIT, frequency, mode)
- Real-time CW keying from your logger
- Speed control and inline speed changes (+/-)
- Special character mapping for prosigns (AR, BT, SK, etc.)

### Configurable Parameters
- Contest mode (pile-up or single-caller)
- Activity level (pile-up density)
- Signal conditions (QRN/QRM/QSB on/off)
- Operator skill levels
- Contest duration
- Your call sign, WPM, pitch, bandwidth
- Serial number generation (time-based or random 4-digit for training)
- WAV file recording for practice sessions
- Automatic QSO logging to CSV files

## Quick Start

### Prerequisites

```bash
# Debian/Ubuntu
sudo apt-get install build-essential cmake librtaudio-dev

# Fedora
sudo dnf install gcc-c++ cmake rtaudio-devel
```

### Build
```bash
cmake .
make
```

### Run
```bash
# WinKeyer protocol (default) - for N1MM+, Win-Test, etc.
./testsim --winkeyer-port 7890

# cwdaemon protocol - for TLF, xlog, etc.
./testsim --cwdaemon-port 6789

# Both protocols simultaneously
./testsim --winkeyer-port 7890 --cwdaemon-port 6789

# With rigctld for RIT control
./testsim --winkeyer-port 7890 --rigctl-port 4532

# Configure your logger to connect:
#   WinKeyer: TCP localhost:7890
#   cwdaemon: UDP localhost:6789
#   rigctld: TCP localhost:4532 (hamlib rig model 2)
#
# For TLF, add to your contest config:
#   CWDAEMON=localhost:6789
#
# Start operating!
```

## Configuration

Create `contest.ini` to customize settings:

```ini
[Station]
call=W1AW
wpm=35
pitch=600
bandwidth=400

[Conditions]
qrn=1
qrm=1
qsb=1
activity=5
lids=1
longnr=1    # Generate 4-digit serial numbers (1-9999) for training

[Contest]
duration=60
mode=RunMode.pileup
savewave=1  # Record audio to timestamped WAV files
```

Run with config:
```bash
./testsim --winkeyer-port 7890 --config contest.ini
./testsim --cwdaemon-port 6789 --config contest.ini
```

See **[WINKEYER_SETUP.md](WINKEYER_SETUP.md)** for detailed logger setup instructions and **[RIGCTL_PROTOCOL.md](RIGCTL_PROTOCOL.md)** for rigctld/RIT control documentation.

## Usage Tips

- **Start with activity=3** for moderate pile-ups
- **Enable lids** to practice with operator mistakes
- **QSB on** makes copying more challenging
- **Single mode** for one-at-a-time QSO practice
- **Adjust bandwidth** to change selectivity (100-600 Hz)
- **Enable longnr=1** to practice copying variable-length serial numbers (70% 3-digit, 30% 4-digit)
- **Enable savewave=1** to record practice sessions to WAV files for later review
- **QSO logs** are automatically saved to CSV files (contest_YYYYMMDD_HHMMSS.log) containing UTC time, callsign, and serial number for each completed contact

## Architecture

Contest Simulator is built from the ground up in C++ with:
- **Station state machines** for realistic operator behavior
- **Audio mixing pipeline** combining multiple simultaneous signals
- **Morse code generation** with proper timing and weighting
- **Signal processing**: AGC, bandwidth filtering, modulation
- **RtAudio** for low-latency audio output

The codebase is inspired by Morse Runner (VE3NEA) and cwsim (W9CF).

## Documentation

- **[CLAUDE.md](CLAUDE.md)** - Architecture and developer documentation
- **[WINKEYER_SETUP.md](WINKEYER_SETUP.md)** - Logger integration guide (WinKeyer/cwdaemon)
- **[RIGCTL_PROTOCOL.md](RIGCTL_PROTOCOL.md)** - Hamlib rigctld protocol support

## Project Status

**Production Ready**: All core functionality is complete:
- ✅ Contest simulator engine
- ✅ WinKeyer2/3 protocol implementation (TCP)
- ✅ cwdaemon protocol implementation (UDP)
- ✅ rigctld protocol implementation (TCP, hamlib-compatible)
- ✅ Audio generation and mixing
- ✅ Station lifecycle management
- ✅ Configuration system
- ✅ CLI daemon application
- ✅ WAV recording and QSO logging
- ✅ No GUI dependencies (Qt/Marble removed)

## License

GPL v3 (inherited from Morse Runner and cwsim)

## Credits

- **Original concept**: Morse Runner by VE3NEA
- **cwsim**: W9CF (Kevin Schmidt)
- **C++ implementation**: This project

## Contributing

This is a personal project. For questions or issues, see the documentation files.

## See Also

- **Morse Runner**: The original Windows-based contest simulator
- **cwsim**: The implementation this is based on
- **cwdaemon**: Morse code keyer daemon for amateur radio programs
