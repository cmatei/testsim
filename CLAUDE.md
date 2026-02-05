# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository. Don't brand commits with coauthorship.

## Project Overview

This is a **CW (Morse code) contest simulator**. The application (Contest Simulator) simulates amateur radio stations participating in contests, generating Morse code audio signals with realistic characteristics like QSB (fading), timing variations, and signal processing.

**Architecture**: The simulator operates as a **virtual contest radio** controlled via **WinKeyer3** or **cwdaemon** protocols, allowing operators to use their preferred contest logging software (N1MM+, Win-Test, TLF, etc.) while practicing against simulated pile-ups.

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
- Network keyer interface (`winkeyer.h`, `winkeyer.cpp`) - WinKeyer3 (TCP) and cwdaemon (UDP) protocols for logger integration
- CLI application (`testsim_daemon.cpp`) - Production daemon
- Configuration file handling (INI reader/writer)

**WAV Recording:**
- WAV file recording (`contest.h`, `contest.cpp`) - Records final mixed audio output to timestamped WAV files
  - Format: 16-bit PCM, mono, sample rate from config (typically 44100 Hz)
  - Activated by `savewave=1` in contest.ini
  - Automatic timestamped filenames (e.g., `contest_20260201_143052.wav`)
  - Recording starts with Contest::start() and stops with Contest::stop()
  - Captures audio after AGC processing (final output quality)

**QSO Logging:**
- QSO text logging (`contest.h`, `contest.cpp`) - Logs completed QSOs to timestamped CSV files
  - Format: CSV with columns: UTC Time, Callsign, Serial Number
  - Automatic timestamped filenames matching WAV files (e.g., `contest_20260201_143052.log`)
  - Only logs valid QSOs (non-zero serial numbers)
  - Logging starts with Contest::start() and stops with Contest::stop()
  - CSV format for easy import into spreadsheets

**Not Implemented:**
- GUI interface (not needed for WinKeyer mode)
- Prefix database for geographic information

## Usage

### Quick Start

1. **Start Contest Simulator:**
```bash
# WinKeyer protocol (default)
./testsim --winkeyer-port 7890

# cwdaemon protocol
./testsim --cwdaemon-port 6789

# Both protocols simultaneously
./testsim --winkeyer-port 7890 --cwdaemon-port 6789

# With rigctld for RIT control
./testsim --winkeyer-port 7890 --rigctl-port 4532
```

2. **Configure your contest logger:**
   - **WinKeyer**: Connect via TCP to `localhost:7890` (Win-Test, DXLog.net, TRX-Manager, N1MM+ with bridge)
   - **cwdaemon**: Connect via UDP to `localhost:6789` (TLF, xlog, CQRLog, Tucnak)
   - **rigctld**: Configure hamlib rig model 2, TCP `localhost:4532` (for RIT control)

3. **Start operating!** Send CQ from your logger and work the simulated pile-up.

**See WINKEYER_SETUP.md for detailed setup instructions and RIGCTL_PROTOCOL.md for rigctld/RIT documentation.**

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
- `testsim` - Network keyer daemon (supports WinKeyer3, cwdaemon, and rigctld protocols)

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
  - Amplitude: uniform distribution from 3000 (very weak) to 50000 (very strong) for realistic signal variation
- `DxOperator`: AI state machine with 8 states (NeedPrevEnd, NeedQso, NeedNr, etc.)
- Edit distance algorithm for call sign matching with "lid" behavior
- Partial call recognition: matches any substring (prefix, middle, or suffix) of the full call
- Realistic pileup behavior: stations go quiet (NeedPrevEnd) when user works someone else, wake up after TU
- Variable patience (minimum 4-8 attempts) and skill levels for realistic operator simulation
- `norepeats` mode: when enabled, stations send compact exchanges without repetition
- `longnr` mode: serial number generation
  - When disabled (0): time-based serials that grow during contest (1 + random × minutes × skill)
  - When enabled (1): random serials for better copy training with realistic distribution:
    - 70% are 3-digit numbers (100-999)
    - 30% are 4-digit numbers (1000-9999)

**Interference Simulation**
- `QrnStation` (`qrnstation.h`, `qrnstation.cpp`): Atmospheric noise (sparse bursts)
- `QrmStation` (`qrmstation.h`, `qrmstation.cpp`): Station interference (CQ calls, QRL?, QSY)

**MyStation** (`mystation.h`, `mystation.cpp`)
- User's station implementation
- **Accumulate-then-detect message parsing**: `sendText()` accumulates text in `full_text` across multiple calls, then `detectAndSetMessages()` parses the complete message to determine type (CQ, TU, exchange, AGN, etc.). Detection happens in main thread to avoid string operations in audio callback.
- `sendText()`: splits text on `<his>` placeholders into pieces for dynamic call updates; accumulates full text and detects message type on each call
- Real-time call sign updates while transmitting
- Abort functionality for stopping mid-transmission (clears accumulated text)
- Contest notifications on transmission start/finish

**Network Keyer Interfaces** (`winkeyer.h`, `winkeyer.cpp`)
- **WinKeyer3 Protocol** (TCP):
  - Implements WinKeyer2/3 protocol over TCP socket
  - Allows Windows contest loggers (N1MM+, Win-Test, etc.) to control the simulator
  - Decodes keyer commands into text for MyStation
  - Reports busy/breakin status back to logger
  - Supports speed changes, PTT, buffer management
  - TcpTransport: TCP network communication with listen/accept
  - Default port: 7890
  - Supports multiple connect/disconnect cycles without restart
- **cwdaemon Protocol** (UDP):
  - Implements cwdaemon protocol over UDP socket
  - Allows Linux contest loggers (TLF, xlog, CQRLog, etc.) to control the simulator
  - ESC-based command protocol for speed, PTT, abort, etc.
  - Inline speed changes with +/- characters: text is split on +/- boundaries, each segment sent separately via `onTextToSend` with `onSpeedChange` between them; MyStation's accumulate-then-detect automatically handles message type detection across split segments; abort (ESC-4) restores WPM to pre-inline-change value (`base_wpm`)
  - Special character mapping for prosigns (AR, BT, SK, etc.)
  - UdpTransport: UDP datagram communication with recvfrom/sendto
  - Default port: 6789
  - Connectionless operation with reply support
- **rigctld Protocol** (TCP):
  - Implements hamlib rigctld protocol over TCP socket
  - Allows contest loggers and other applications to control simulator as a transceiver
  - Primary use case: RIT (Receiver Incremental Tuning) control for frequency shifts
  - Also supports: frequency, mode, VFO, PTT queries (cosmetic except RIT)
  - Short command format (single character) and long format (backslash-prefixed)
  - Standard hamlib RPRT response codes for error handling
  - TcpTransport: TCP network communication with listen/accept
  - Default port: disabled (use --rigctl-port to enable, typically 4532)
  - RIT bidirectionally synced with Contest class, other state internal to RigctldServer
  - dump_state command for capability reporting
- **Transport Abstraction**: Common interface for TCP and UDP with polymorphism
- All three protocols can run simultaneously on different ports

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
   - Phase accumulation maintains continuity within buffers (unwrapped phase)
   - Only `fbfo` is wrapped to [0, 2π) at buffer boundaries for next buffer
   - Ensures click-free audio across full ±5000 Hz frequency range
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
- Audio generation uses phase accumulation with verified continuity (±5000 Hz range, zero discontinuities)
- AGC implementation complete with logarithmic compression and shaped envelope
- QSB fading uses three-stage MovAvg filters for Gaussian process
- DxStation/DxOperator provide realistic contest participant simulation:
  - Partial call recognition (substring matching at any position)
  - Realistic pileup behavior:
    - Stations not yet engaged back off (NeedPrevEnd) when hearing MyStation transmit
    - Stations already engaged in QSO (NeedNr/NeedCall/NeedCallNr/NeedEnd) continue listening
    - Prevents interference with ongoing QSOs while allowing active QSOs to complete
    - Wake up on CQ, TU, or NIL messages (or if they hear their own call)
  - Variable patience (4-8 minimum attempts), skill levels, "lid" behavior
  - **Patience reset**: When a station hears their call (full or partial match), patience resets to maximum (8) - simulates operator confidence when being called
  - **AGN/NR? handling**: Stations repeat their exchange when receiving AGN or NR? messages, with patience reset to maximum
  - norepeats mode for compact exchanges
- QRN creates sparse atmospheric noise bursts (99% zero samples, 10^5-10^7 amplitude)
- QRM creates persistent station interference with variable patience (1-5 transmissions)
- MyStation enables dynamic call sign updates while transmitting:
  - Piece-based message sending with `<his>` placeholders
  - Accumulate-then-detect message parsing (main thread, not audio callback)
  - Real-time call updates during transmission
- Contest class provides complete audio mixing pipeline:
  - White noise + QRN spikes
  - Multi-station audio mixing with RIT support (optimized trig calculations)
  - QSK RF gain control with fast attack/slow decay
  - 3-stage complex bandwidth filtering
  - Modulation and AGC
  - Automatic DxStation creation based on activity level (Poisson distribution)
  - Deferred station creation (atomic flags, main thread allocation)
  - Station lifecycle management (create/tick/remove)
  - QSO completion tracking via queue
- WAV file recording captures final mixed audio:
  - 16-bit PCM WAV format with automatic timestamp-based filenames
  - Records audio after AGC processing (in audio callback thread)
  - Controlled by savewave config parameter
  - Header written on file open, updated with correct sizes on close
  - Direct write from audio callback (buffered by std::ofstream)
- QSO logging to CSV files:
  - Automatic timestamped CSV files matching WAV filenames
  - Logs UTC timestamp, callsign, serial number for each completed QSO
  - Only logs valid QSOs (non-zero serial numbers)
  - Written from audio callback when QSO completes (OperatorState::Done)
  - Immediate flush for data integrity

**Bug Fixes Applied:**
- MovAvg buffer size corrected from `bufsize + navg - 1` to `bufsize + navg`
- This fix applies to both real and complex-valued moving average filters
- `station::get_buffer()` buffer overflow fixed: changed `std::max` to `std::min` on line 81
  - Bug was causing crashes after ~3 seconds when audio buffer copy exceeded vector bounds
  - Loop was attempting to read beyond `envelope.size()` and write beyond `buffer` capacity
- `Modulator::modulate()` fixed: changed `resize(bufsize)` to `reserve(bufsize)` to prevent double-size output vector when combined with `push_back`
- Thread safety: added `std::mutex` to Contest class protecting shared state accessed from both the RtAudio callback thread and the main thread (`getAudio()`, `dxCount()`, `setCall()`, `setWpm()`, `time()`). Note: mutex locks in `onMeStartedSending()` and `onMeFinishedSending()` are currently disabled
- `Contest::setTqrm()` integer division fix: `_bufsize / _rate / _tqrm` (all integers) always truncated to 0, preventing QRM from ever triggering; fixed with float cast
- `RNG::integers()` off-by-one: `random()` returning exactly 1.0 could produce `high` as result, causing out-of-bounds array access; added clamp to `high - 1`
- `Contest::time()` added mutex lock for thread safety
- `MyStation::updateCallInMessage()` added `envelope.size()` bounds check in comparison loop
- `Keyer::setRisetime()` replaced float accumulation loop with integer loop to avoid floating-point drift
- `QrmStation` constructor: added explicit `static_cast<int>` for `size_t` to `int` conversion
- `station::tick()` added `timeout != NEVER` guard to prevent useless decrementing of INT32_MAX
- **BFO phase discontinuity fix** (`station::get_bfo()`): Critical audio quality fix eliminating clicks/pops in generated audio
  - Original bug: phase wrapping during loop created jumps up to 150 radians between samples
  - Fix: keep `bfo[]` values continuous (unwrapped) within each buffer, only wrap `fbfo` at buffer boundaries
  - Verified across ±5000 Hz range with 414,639+ continuity checks (100% pass rate)
  - Phase errors now at machine precision (~10^-7 radians)
- **MyStation message detection refactoring**: Changed from dual-entry-point to accumulate-then-detect pattern
  - Removed public `detectMessage()` method
  - Added private `detectAndSetMessages()` called from main thread (not audio callback)
  - `sendText()` accumulates text in `full_text` across multiple calls
  - Detection uses early-return pattern for clarity
  - Prevents string operations in audio callback thread
- **DxOperator behavior improvements**: Realistic pileup simulation
  - Partial call matching: changed from prefix-only to substring matching (any position in call)
  - Stations go quiet (NeedPrevEnd) when user works someone else, preventing interference
  - **mestarted event fix**: Stations in NeedPrevEnd or NeedQso states automatically back off when hearing MyStation transmit (assume busy with another QSO); stations already engaged (NeedNr/NeedCall/NeedCallNr/NeedEnd) continue listening to complete their QSO - prevents interference while allowing QSO completion
  - **mefinished state management fix**: Stations in NeedPrevEnd state now stay in listening state instead of moving to preparingtosend - prevents calling when they should be waiting quietly for ongoing QSO to finish
  - **Patience reset on call heard**: When station hears their call (full or partial match), patience resets to FULL_PATIENCE (8) - simulates operator confidence and persistence when being called
  - **AGN/NR? message handling**: Added handling for AGN and NR? messages - stations now repeat their exchange (by setting repeatCnt=2) and reset patience to maximum when receiving repeat requests
  - TU message properly wakes up waiting stations
  - Partial call replies never include NR (just send call again), regardless of norepeats mode
  - Patience initialization: minimum 4-8 attempts (was 0-∞) prevents premature station removal
  - Station creation: added `mefinished` event to new stations so they start calling immediately
- **DxStation amplitude variability**: Changed from concentrated mid-range (9000-45000 with sinusoidal distribution) to wide uniform distribution (3000-50000) for more realistic signal strength variation and better training exposure to weak and strong signals
- **Performance optimizations**: Deferred station creation to avoid heap allocation in audio callback
  - Atomic flags `_pendingStations` and `_pendingIsSingle` signal main thread
  - `createPendingStations()` called from main loop, not audio callback
  - Pre-allocated vectors: `stations.reserve(100)`, `msgs.reserve(4)`
  - Optimized RIT phase calculation: replaced `std::exp()` with `cos()/sin()`

**Future Enhancements (Optional):**
- Prefix database for call sign geography
- Statistics and performance analysis tools
- WAV recording enhancements:
  - Optional FLAC compression to save disk space
  - Manual control via signal handlers (SIGUSR1)
  - Ring buffer for real-time thread safety
  - RIFF INFO metadata (call sign, mode, date)
  - Auto-rotation for long contests
