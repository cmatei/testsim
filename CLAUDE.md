# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository. Don't brand commits with coauthorship.

## Project Overview

This is a **CW (Morse code) contest simulator**. The application (Contest Simulator) simulates amateur radio stations participating in contests, generating Morse code audio signals with realistic characteristics like QSB (fading), timing variations, and signal processing.

**Architecture**: The simulator operates as a **virtual contest radio** controlled via **WinKeyer3 protocol**, allowing operators to use their preferred contest logging software (N1MM+, Win-Test, etc.) while practicing against simulated pile-ups.

**Source Material**: Based on Morse Runner by VE3NEA, licensed under GPL v3.

### Components

**Implementation:**
- Station base class (`station.h`, `station.cpp`) - State machine, message handling, audio generation
- Morse code generation (`keyer.h`, `keyer.cpp`) - Complete with timing and envelopes
- Audio processing (`audioprocess.h`, `audioprocess.cpp`) - MovAvg, Modulator, AGC
- QSB fading simulation (`qsb.h`, `qsb.cpp`) - Complete with Gaussian process generation
- Call sign database (`calllist.h`, `calllist.cpp`) - Loads MASTER.SCP, random selection
- Random number generation (`random.h`, `random.cpp`) - Uniform, normal, Rayleigh, Poisson distributions
- DxStation (`dxstation.h`, `dxstation.cpp`) - Simulated contest stations with full QSO logic
- DxOperator (`dxoper.h`, `dxoper.cpp`) - AI state machine for operator behavior
- QrnStation (`qrnstation.h`, `qrnstation.cpp`) - Atmospheric noise simulation
- QrmStation (`qrmstation.h`, `qrmstation.cpp`) - Station interference simulation
- MyStation (`mystation.h`, `mystation.cpp`) - User's station with dynamic call updates
- Contest class (`contest.h`, `contest.cpp`) - Core orchestrator with audio mixing and RtAudio integration
- WinKeyer3 interface (`winkeyer.h`, `winkeyer.cpp`) - Serial protocol for logger integration
- CLI application (`testsim_daemon.cpp`) - Production daemon
- Configuration file handling (INI reader/writer)

**Not Implemented:**
- GUI interface (not needed for WinKeyer mode)
- Prefix database for geographic information

## Usage

### Quick Start

1. **Start Contest Simulator:**
```bash
./testsim --port 7890
```

2. **Configure your contest logger:**
   - Connect to WinKeyer via TCP: `localhost:7890`
   - Some loggers call this "Network WinKeyer" or "TCP WinKeyer"
   - Examples: Win-Test (network mode), DXLog.net, TRX-Manager

3. **Start operating!** Send CQ from your logger and work the simulated pile-up.

**Note:** Some loggers (like N1MM+) may require third-party TCP-to-serial bridge software if they don't support network WinKeyer natively.

**See WINKEYER_SETUP.md for detailed setup instructions.**

## Build System

### Building the Application

```bash
cmake .
make
```

**Dependencies**:
- RtAudio library (real-time audio output)
- CMake 3.16+
- C++17 compiler
- Linux serial port support (termios)
- Standard C/C++ libraries

**Executable**:
- `testsim` - WinKeyer3 daemon mode (434KB binary)

**Note**: Qt and Marble have been completely removed from the project. This is a CLI-only application.

### Generating Wavetables

The `gentables` utility generates wavetable lookup tables for audio oscillators:

```bash
gcc -o gentables gentables.c -lm
./gentables > wavetables.cpp
```

## Architecture

### Core Components

**Station System** (`station.h`, `station.cpp`)
- Base class for simulating radio stations
- State machine with states: listening, copying, preparingtosend, sending, deleteme
- Event-driven architecture processing: timeout, msgsent, mestarted, mefinished
- Pure virtual `processEvent()` must be implemented by derived classes
- Handles message queueing, timing, and transmission logic
- Messages use template strings with `<my>`, `<his>`, `<#>` placeholders replaced at runtime

**Morse Code Generation** (`keyer.h`, `keyer.cpp`)
- Converts text to Morse code with proper timing
- Generates shaped envelopes with configurable rise/fall times
- Default sample rate: 11025 Hz, buffer size: 512 samples
- Uses static morse code mapping table

**Audio Signal Processing** (`audioprocess.h`, `audioprocess.cpp`)
- `MovAvg`: Complex-valued moving average filter for bandwidth filtering and QSB generation
- `Modulator`: Modulates complex signals to audio frequency with pitch control
- `Agc`: Automatic Gain Control - logarithmic compression with shaped attack/hold/decay

**QSB (Fading) Simulation** (`qsb.h`, `qsb.cpp`)
- Simulates realistic signal fading effects using Gaussian process
- Uses 3-stage `MovAvg` filters for smooth correlated gain variations
- Configurable bandwidth, linear interpolation for smooth transitions

**DxStation System** (`dxstation.h`, `dxstation.cpp`, `dxoper.h`, `dxoper.cpp`)
- `DxStation`: Simulated contest station with QSB, variable pitch, amplitude, WPM
- `DxOperator`: AI state machine with 8 states (NeedPrevEnd, NeedQso, NeedNr, etc.)
- Edit distance algorithm for call sign matching with "lid" behavior
- Variable patience and skill levels for realistic operator simulation

**Interference Simulation**
- `QrnStation` (`qrnstation.h`, `qrnstation.cpp`): Atmospheric noise (sparse bursts)
- `QrmStation` (`qrmstation.h`, `qrmstation.cpp`): Station interference (CQ calls, QRL?, QSY)

**MyStation** (`mystation.h`, `mystation.cpp`)
- User's station implementation
- Message piece splitting on `<his>` placeholders for dynamic call updates
- Real-time call sign updates while transmitting
- Abort functionality for stopping mid-transmission
- Contest notifications on transmission start/finish

**WinKeyer3 Interface** (`winkeyer.h`, `winkeyer.cpp`)
- Implements WinKeyer3 protocol over TCP socket
- Allows contest loggers (N1MM+, Win-Test, etc.) to control the simulator over the network
- Decodes keyer commands into text for MyStation
- Reports busy/breakin status back to logger
- Supports speed changes, PTT, buffer management
- Transport layer abstraction: TcpTransport for network communication
- Network socket server listening on configurable port (default: 7890)
- Supports multiple connect/disconnect cycles without restart

**Contest Class** (`contest.h`, `contest.cpp`)
- Main orchestrator that ties all components together
- Audio mixing: combines buffers from MyStation + all DxStations + QRN + QRM
- RtAudio integration: real-time audio streaming with callback
- Station lifecycle: creates DxStations based on activity (Poisson distribution)
- QSK (full break-in): fast attack/slow decay RF gain control
- Bandwidth filtering: 3-stage complex moving average
- Configuration: INI file reading/writing with default values
- Run modes: stop, pileup, single, pileup_qsonr, single_qsonr
- Event distribution: notifies all stations of user's transmission state
- Time tracking: bufcount, seconds, duration checking

**Call Sign Database** (`calllist.h`, `calllist.cpp`)
- Loads call signs from MASTER.SCP file
- Provides random selection for realistic contest simulation

**Random Number Generation** (`random.h`, `random.cpp`)
- Custom RNG wrapper for consistent randomization across components
- Distributions: uniform, normal, Rayleigh, Poisson

### Audio Pipeline

1. Text message → Morse encoder (Keyer)
2. Morse pattern → Envelope shaper (rise/fall times)
3. Envelope → BFO (Beat Frequency Oscillator) generation with configurable pitch
4. Buffer-based audio generation for RtAudio streaming
5. Optional AGC and filtering (MovAvg, Agc classes)

### Data Flow

Station objects maintain:
- Internal state machines for contest QSO flow
- Audio buffers (envelope, bfo, buffer vectors)
- Message queues with template-based text generation
- Timing mechanisms (timeout values, NEVER constant for disabled timeouts)

The `station::get_buffer()` method provides audio data in chunks, managing the send position and clearing the envelope when complete.

### Station Types

**Base Station** (`station.h`, `station.cpp`)
- Abstract base class with pure virtual `processEvent()`
- Manages state machine, message queue, audio buffers, timing
- Provides morse code transmission infrastructure

**DxStation** (`dxstation.h`, `dxstation.cpp`)
- Simulated contest participant
- Uses DxOperator for AI behavior
- Includes QSB fading, random pitch/amplitude/WPM
- Variable "lid" behavior (errors in RST, serial numbers)
- Lifecycle: copying → preparingtosend → sending → deleteme

**QrnStation** (`qrnstation.h`, `qrnstation.cpp`)
- Atmospheric noise interference
- Single burst: 1-2 seconds, 10^5-10^7 amplitude
- 99% sparse (creates "crash" sound)
- Lifecycle: sending → msgsent → deleteme

**QrmStation** (`qrmstation.h`, `qrmstation.cpp`)
- Station interference (other operators)
- Sends QRL?, LongCQ, QSY messages
- Patience: 1-5 transmissions, 2-6 second delays
- Amplitude: 5000-30000, Pitch: ±300 Hz, WPM: 30-50
- Lifecycle: sending → timeout/msgsent cycle → deleteme

## Key Constants and Conventions

- `NEVER = INT32_MAX`: Indicates disabled timeout
- Default audio rate: 11025 Hz
- Default buffer size: 512 samples
- Station pitch typically 500 Hz (BFO frequency)
- WPM (words per minute): ~30 default
- Number format: RST + serial number (e.g., "599 001"), with substitutions like N=9, T=0, O=0

## Implementation Notes

**Completed Features:**
- Audio generation uses phase accumulation to avoid discontinuities
- AGC implementation complete with logarithmic compression and shaped envelope
- QSB fading uses three-stage MovAvg filters for Gaussian process
- DxStation/DxOperator provide realistic contest participant simulation
- QRN creates sparse atmospheric noise bursts (99% zero samples, 10^5-10^7 amplitude)
- QRM creates persistent station interference with variable patience (1-5 transmissions)
- MyStation enables dynamic call sign updates while transmitting via piece-based message sending
- Contest class provides complete audio mixing pipeline:
  - White noise + QRN spikes
  - Multi-station audio mixing with RIT support
  - QSK RF gain control with fast attack/slow decay
  - 3-stage complex bandwidth filtering
  - Modulation and AGC
  - Automatic DxStation creation based on activity level (Poisson distribution)
  - Station lifecycle management (create/tick/remove)
  - QSO completion tracking via queue

**Bug Fixes Applied:**
- MovAvg buffer size corrected from `bufsize + navg - 1` to `bufsize + navg`
- This fix applies to both real and complex-valued moving average filters
- `station::get_buffer()` buffer overflow fixed: changed `std::max` to `std::min` on line 81
  - Bug was causing crashes after ~3 seconds when audio buffer copy exceeded vector bounds
  - Loop was attempting to read beyond `envelope.size()` and write beyond `buffer` capacity
- `Modulator::modulate()` fixed: changed `resize(bufsize)` to `reserve(bufsize)` to prevent double-size output vector when combined with `push_back`
- Thread safety: added `std::mutex` to Contest class protecting shared state accessed from both the RtAudio callback thread and the main thread (`getAudio()`, `dxCount()`, `onMeStartedSending()`, `onMeFinishedSending()`, `setCall()`, `setWpm()`, `time()`)
- `Contest::setTqrm()` integer division fix: `_bufsize / _rate / _tqrm` (all integers) always truncated to 0, preventing QRM from ever triggering; fixed with float cast
- `RNG::integers()` off-by-one: `random()` returning exactly 1.0 could produce `high` as result, causing out-of-bounds array access; added clamp to `high - 1`
- `Contest::time()` added mutex lock for thread safety
- `MyStation::updateCallInMessage()` added `envelope.size()` bounds check in comparison loop
- `Keyer::setRisetime()` replaced float accumulation loop with integer loop to avoid floating-point drift
- `QrmStation` constructor: added explicit `static_cast<int>` for `size_t` to `int` conversion
- `station::tick()` added `timeout != NEVER` guard to prevent useless decrementing of INT32_MAX

**Future Enhancements (Optional):**
- Prefix database for call sign geography
- WAV file recording of contest audio
- Statistics and performance analysis tools
