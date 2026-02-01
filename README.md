# Contest Simulator - Contest Simulator for CW Operators

A realistic CW (Morse code) contest simulator that integrates with popular contest logging software via WinKeyer3 protocol.

## What is Contest Simulator?

Contest Simulator simulates a complete contest environment with:
- **Realistic pile-ups** with multiple calling stations
- **Signal propagation effects**: QSB (fading), QRN (atmospheric noise), QRM (interference)
- **Operator behavior simulation**: Mistakes, impatience, lid behavior
- **Real logging software integration**: Use N1MM+, Win-Test, or any WinKeyer-compatible logger

Instead of a built-in GUI, Contest Simulator acts as a **virtual contest radio** that your logging software controls, providing a more realistic practice environment.

## Features

### Realistic Simulation
- Multiple DX stations call simultaneously (pile-up mode)
- Signal fading (QSB) with Gaussian process modeling
- Atmospheric noise bursts (QRN)
- Station interference (QRM)
- Operator mistakes and varying skill levels
- Variable sending speeds and timing

### Logger Integration
- WinKeyer3 USB protocol over virtual serial port
- Works with N1MM+, Win-Test, TR4W, and other WinKeyer-compatible loggers
- Real-time CW keying from your logger
- Busy/breakin status reporting
- Speed synchronization

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
sudo apt-get install build-essential cmake librtaudio-dev socat

# Fedora
sudo dnf install gcc-c++ cmake rtaudio-devel socat
```

### Build
```bash
cd /home/cmatei/qt-tests
cmake .
make
```

### Run
```bash
# Terminal 1: Create virtual serial ports
socat -d -d pty,raw,echo=0,link=/tmp/logger pty,raw,echo=0,link=/tmp/testsim

# Terminal 2: Start Contest Simulator
./testsim --serial /tmp/testsim

# Configure your logger to use /tmp/logger as WinKeyer port
# Start operating!
```

**For detailed setup instructions, see [WINKEYER_SETUP.md](WINKEYER_SETUP.md)**

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
./testsim --serial /tmp/testsim --config contest.ini
```

## Usage Tips

- **Start with activity=3** for moderate pile-ups
- **Enable lids** to practice with operator mistakes
- **QSB on** makes copying more challenging
- **Single mode** for one-at-a-time QSO practice
- **Adjust bandwidth** to change selectivity (100-600 Hz)
- **Enable longnr=1** to practice copying 4-digit serial numbers (1-9999)
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

- **[WINKEYER_SETUP.md](WINKEYER_SETUP.md)** - Detailed setup guide
- **[CLAUDE.md](CLAUDE.md)** - Architecture and developer documentation

## Project Status

**Production Ready**: All core functionality is complete:
- ✅ Contest simulator engine
- ✅ WinKeyer3 protocol implementation
- ✅ Audio generation and mixing
- ✅ Station lifecycle management
- ✅ Configuration system
- ✅ CLI daemon application
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
- **WinKeyer**: K1EL's hardware CW keyer and protocol standard
