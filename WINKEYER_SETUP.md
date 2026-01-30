# Network Keyer Setup Guide

Contest Simulator operates as a virtual contest radio controlled by your logging software via network keyer protocols (WinKeyer3 or cwdaemon).

## Protocols

Contest Simulator supports two network keyer protocols:

- **WinKeyer3** (TCP, port 7890) - Windows loggers: N1MM+, Win-Test, DXLog.net, TRX-Manager
- **cwdaemon** (UDP, port 6789) - Linux loggers: TLF, xlog, CQRLog, Tucnak

## Quick Start

### 1. Start Contest Simulator

**For WinKeyer (default):**
```bash
./testsim --port 7890
```

**For cwdaemon:**
```bash
./testsim --protocol cwdaemon --cwdaemon-port 6789
```

**For both protocols simultaneously:**
```bash
./testsim --protocol both --port 7890 --cwdaemon-port 6789
```

You should see:
```
=====================================
      Contest Simulator
=====================================

Initializing contest simulator...
Configuration:
  Call:      YO3GEK
  WPM:       40
  Mode:      Pileup
  Activity:  6
  ...

WinKeyer3: TCP server listening on port 7890
Configure your logger to connect to: localhost:7890

=== Contest Running ===
WinKeyer3 listening on: localhost:7890
Press Ctrl+C to stop
```

### 2. Configure Your Contest Logger

#### cwdaemon Loggers (Linux)

##### TLF (The Logging Framework)

TLF has native cwdaemon support:

1. Edit your TLF contest configuration file (e.g., `cqww.ini`)
2. Add or modify:
   ```
   CWDAEMON=localhost:6789
   ```
3. Start TLF: `tlf -d cqww`
4. TLF will automatically connect to cwdaemon

##### xlog

1. **Settings** → **Preferences** → **Keying**
2. Select **cwdaemon** as keyer type
3. Host: `localhost`
4. Port: `6789`
5. Click **Test** to verify connection

##### CQRLog

1. **Preferences** → **TRX Control**
2. CW Keyer: **cwdaemon**
3. Host: `localhost:6789`
4. Apply and test

#### WinKeyer Loggers (Windows/Cross-platform)

#### Win-Test (Recommended)

Win-Test has native support for network WinKeyer:

1. **Options** → **Interfaces** → **Winkeyer**
2. Select **Network** mode
3. Enter host: `localhost` or `127.0.0.1`
4. Enter port: `7890`
5. Click **Connect**
6. You should see "WinKeyer connected" in status bar

#### DXLog.net

DXLog.net supports TCP WinKeyer:

1. **Options** → **Station** → **CW/WinKeyer**
2. Enable **Network WinKeyer**
3. Host: `localhost`
4. Port: `7890`
5. Test connection

#### TRX-Manager

TRX-Manager can connect via TCP:

1. **Settings** → **CW Keyer** → **WinKeyer**
2. Select **Network** connection
3. Host: `localhost:7890`
4. Apply settings

#### N1MM+ (Requires Bridge)

N1MM+ doesn't support network WinKeyer natively. You have two options:

**Option A: Use com0com + tcp2com**
1. Install [com0com](https://sourceforge.net/projects/com0com/) (virtual serial ports)
2. Install [tcp2com](https://github.com/pyserial/pyserial/tree/master/examples) or similar TCP-to-serial bridge
3. Create virtual port pair (e.g., COM1 ↔ COM2)
4. Run: `tcp2com --port 7890 --device COM1`
5. Configure N1MM+ WinKeyer to use COM2

**Option B: Use hub4com**
1. Install [hub4com](https://sourceforge.net/projects/com0com/)
2. Run: `hub4com --create-filter=escparse,com,parse --octs=off \\.\COM8 --use-driver=tcp *localhost:7890`
3. Configure N1MM+ WinKeyer to use COM8

#### Other Loggers

Look for these settings:
- "Network WinKeyer" or "TCP WinKeyer"
- "Remote WinKeyer"
- "Ethernet Keyer"

Connect to: `localhost:7890`

### 3. Audio Routing

Contest Simulator outputs audio to the default system audio device. Your logger needs to receive this audio as input.

#### Option A: Same Sound Card (Simplest)

If your logger supports selecting audio input device:
1. Contest Simulator outputs to default speakers/headphones
2. Logger uses the same device for input (most sound cards support monitoring)
3. You'll hear the pile-up in your headphones

#### Option B: Virtual Audio Cable (PulseAudio)

For separate audio routing on Linux:

```bash
# Create virtual audio sink
pactl load-module module-null-sink sink_name=testsim_audio sink_properties=device.description="ContestSim_Audio"

# Create loopback to make it an input source
pactl load-module module-loopback sink=testsim_audio source=testsim_audio.monitor

# Route testsim to the virtual sink using pavucontrol
pavucontrol
```

Then configure your logger to use "ContestSim_Audio" as audio input.

#### Option C: Virtual Audio Cable (JACK)

For professional audio routing:

```bash
# Start JACK if not running
qjackctl &

# Launch testsim (will appear in JACK connections)
./testsim --port 7890

# Use qjackctl to route testsim output to logger input
```

## Usage

### Operating

Once everything is connected:

1. **Start contest in logger** (File → New Log, enter contest)
2. **Send CQ** using your logger's CQ function key (usually F1)
   - Contest Simulator transmits the CQ
   - Simulated stations hear it and start calling
3. **You hear the pile-up** in your audio
4. **Click on a caller** in the bandmap or type their call
5. **Send exchange** (usually F2 or F3)
6. **Log the QSO** when complete

Contest Simulator simulates:
- Multiple stations calling (pile-up)
- Signal fading (QSB)
- Atmospheric noise (QRN)
- Station interference (QRM)
- Realistic operator behavior (lids, mistakes, impatience)

### Status Display

The Contest Simulator terminal shows:
```
[00:05:23] QSOs: 12 DX: 3 WPM: 35 TX
```

- **Time**: Contest elapsed time
- **QSOs**: Completed QSOs
- **DX**: Currently active DX stations
- **WPM**: Current sending speed
- **TX/RX**: Transmit or receive state

### Completed QSOs

As QSOs complete, they're printed to the console:
```
[QSO] W1ABC 599 042
[QSO] K3XYZ 599 043
```

### Stopping

Press `Ctrl+C` to stop Contest Simulator. It will show final statistics:
```
Final Statistics:
  Time: 0h 15m 42s
  QSOs: 87
```

## Configuration

### Command Line Options

```bash
./testsim --help

Options:
  --protocol <winkeyer|cwdaemon|both>  Protocol to use (default: winkeyer)
  --port <port>                        TCP port for WinKeyer (default: 7890)
  --cwdaemon-port <port>               UDP port for cwdaemon (default: 6789)
  --winkeyer-version <2|3>             WinKeyer protocol version (default: 3)
  --config <file>                      Configuration file (default: none)
```

Examples:
```bash
# WinKeyer on default port 7890
./testsim

# cwdaemon on default port 6789
./testsim --protocol cwdaemon

# Both protocols simultaneously
./testsim --protocol both

# Custom ports
./testsim --protocol both --port 8000 --cwdaemon-port 6790

# With configuration file
./testsim --protocol cwdaemon --config contest.ini
```

### Configuration File (Optional)

Create `contest.ini`:

```ini
[Appearance]
fontsize=12

[Sound]
rate=11025
bufsize=512

[Station]
call=P55CF
wpm=40
fast=1.1
slow=0.9
bandwidth=500
pitch=500
qsk=1
qskdecaytime=0.030
cwreverse=0
rit=0
monitor=0.1

[Conditions]
qrn=1
qrm=1
tqrm=240
qsb=1
flutter=1
qsy=1
lids=1
activity=4
lidrstprob=0.03
lidnrprob=0.1
rptprob=0.1
flutterprob=0.3

[Contest]
duration=60
mode=RunMode.pileup
savewave=0
saveini=1
savesummary=1
```

Run with config:
```bash
./testsim --port 7890 --config contest.ini
```

### Key Parameters

- **call**: Your call sign
- **wpm**: Initial WPM (logger can override)
- **bandwidth**: Receiver bandwidth (100-600 Hz)
- **pitch**: CW pitch (Hz)
- **activity**: Pile-up intensity (1-10, default 4)
- **qrn**: Atmospheric noise on/off
- **qrm**: Station interference on/off
- **qsb**: Signal fading on/off
- **lids**: Enable operator mistakes
- **mode**: `RunMode.pileup` or `RunMode.single`
- **duration**: Contest duration in minutes

## cwdaemon Protocol Features

The cwdaemon protocol support includes:

### Supported Commands

| Command | Function | Implementation |
|---------|----------|----------------|
| `ESC 0` | Reset to defaults | Resets WPM to 24, clears state |
| `ESC 2 <wpm>` | Set speed | Sets WPM (5-60) |
| `ESC 3 <freq>` | Set sidetone | Accepted but ignored (simulator controls audio) |
| `ESC 4` | Abort message | Stops current transmission |
| `ESC 7 <weight>` | Set weighting | Accepted but ignored |
| `ESC a <state>` | PTT control | Sets PTT on/off |
| Plain text | Send CW | Transmits message |

### Inline Speed Changes

Within text messages, you can use:
- `+` - Increase speed by 2 WPM
- `-` - Decrease speed by 2 WPM

Example: `TEST++FASTER--SLOWER` will send "TEST" at base speed, "FASTER" 4 WPM faster, and "SLOWER" back at base speed.

### Special Character Mapping

| Character | Prosign | Morse |
|-----------|---------|-------|
| `*` | AR | `<AR>` |
| `=` | BT | `<BT>` |
| `<` | SK | `<SK>` |
| `(` | KN | `<KN>` |
| `!` | SN | `<SN>` |
| `&` | AS | `<AS>` |
| `>` | BK | `<BK>` |

Example: `CQ*=` sends "CQ" followed by AR and BT prosigns.

### Testing cwdaemon

You can test the cwdaemon interface using command-line tools:

```bash
# Send text message
echo "CQ TEST" | socat - UDP:127.0.0.1:6789

# Set speed to 30 WPM
printf "\x1b\x32\x1e" | socat - UDP:127.0.0.1:6789

# Abort transmission
printf "\x1b\x34" | socat - UDP:127.0.0.1:6789

# Test inline speed changes
echo "TEST++FASTER" | socat - UDP:127.0.0.1:6789
```

## Troubleshooting

### "Connection refused" in logger

```bash
# Check testsim is running
ps aux | grep testsim

# Check port is listening
netstat -an | grep 7890
# or
ss -tlnp | grep 7890

# Try different port
./testsim --port 8000
```

### "Port already in use"

```bash
# Find what's using the port
lsof -i :7890

# Kill the process or use different port
./testsim --port 7891
```

### Logger disconnects immediately

1. Check logger supports network WinKeyer mode
2. Try different logger (Win-Test has excellent support)
3. Check firewall isn't blocking localhost connections
4. Enable debug output if available in logger

### "No audio" or "Can't hear pile-up"

```bash
# Check testsim is outputting audio
pactl list sink-inputs

# Verify default audio device
pactl info | grep "Default Sink"

# Test audio
speaker-test -t sine -f 440

# Check audio in contest config
./testsim --config contest.ini
# Verify rate=11025, bufsize=512
```

### Latency Issues

If you experience audio lag:
1. Reduce buffer size in contest.ini: `bufsize=256` (default 512)
2. Close other audio applications
3. Use ALSA or JACK instead of PulseAudio for lower latency
4. Increase system audio buffer if using JACK

### WinKeyer not responding

Check terminal output for:
```
WinKeyer3: Client connected from 127.0.0.1:xxxxx
WinKeyer3: Initialized
WinKeyer3: Speed set to XX WPM
```

If not seeing connection:
1. Verify logger is configured for network mode
2. Check correct host (localhost or 127.0.0.1) and port
3. Test with simple telnet: `telnet localhost 7890`
4. Check for firewall rules blocking localhost

## Network Operation

### Running on Different Machine

Contest Simulator can run on a different computer:

```bash
# On server machine (192.168.1.100)
./testsim --port 7890

# Configure logger to connect to:
# Host: 192.168.1.100
# Port: 7890
```

**Note:** Make sure firewall allows incoming connections on port 7890.

### Firewall Configuration

Allow incoming TCP connections:

```bash
# UFW (Ubuntu)
sudo ufw allow 7890/tcp

# firewalld (Fedora/RHEL)
sudo firewall-cmd --add-port=7890/tcp --permanent
sudo firewall-cmd --reload

# iptables
sudo iptables -A INPUT -p tcp --dport 7890 -j ACCEPT
```

## Tips

- **Adjust activity** (1-10) for realistic pile-up density
- **Enable lids** for practice with operator mistakes
- **QSB on** makes copying more challenging
- **Single mode** for practicing one-at-a-time QSOs
- **Lower bandwidth** (200-300 Hz) for more realistic filtering
- **Use configuration file** to save your preferred settings
- **Test with Win-Test** first - it has excellent network WinKeyer support

## Support

For issues or questions, see the main README.md or check the documentation in CLAUDE.md.
