# WinKeyer3 Mode Setup Guide

Contest Simulator can operate as a virtual contest radio controlled by your logging software via WinKeyer3 protocol.

## Quick Start

### 1. Create Virtual Serial Port Pair

```bash
# Install socat if needed
sudo apt-get install socat  # Debian/Ubuntu
sudo dnf install socat      # Fedora

# Create virtual serial port pair
socat -d -d pty,raw,echo=0,link=/tmp/logger pty,raw,echo=0,link=/tmp/testsim
```

This creates two linked serial ports:
- `/tmp/logger` - Point your contest logger here
- `/tmp/testsim` - Contest Simulator daemon connects here

**Keep this terminal open** - closing it destroys the virtual ports.

### 2. Start Contest Simulator Daemon

In a new terminal:

```bash
cd /home/cmatei/qt-tests
./testsim --serial /tmp/testsim
```

You should see:
```
=====================================
  Contest Simulator Contest Simulator (WinKeyer3)
=====================================

Initializing contest simulator...
Configuration:
  Call:      P55CF
  WPM:       40
  Mode:      Pileup
  Activity:  4
  ...

=== Contest Running ===
Connect your contest logger to: /tmp/testsim
Press Ctrl+C to stop
```

### 3. Configure Your Contest Logger

#### N1MM+ Configuration
1. **Config** → **Config Ports**
2. **CW/Other** tab
3. Set **Winkeyer Port** to `/tmp/logger`
4. Set **Winkeyer Mode** to WK2 or WK3
5. Click **Test** - you should see "WinKeyer OK"

#### Win-Test Configuration (via Wine)
1. **Options** → **Interfaces** → **Winkeyer**
2. Set port to `COM1` (configure Wine to map `/tmp/logger` to COM1)
3. Test connection

#### Other Loggers
Configure WinKeyer port to point to `/tmp/logger`

### 4. Audio Routing

Contest Simulator outputs audio to the default system audio device. Your logger needs to receive this audio as input.

#### Option A: Use Same Sound Card (Recommended)
If your logger supports selecting audio input device:
1. Contest Simulator outputs to default speakers/headphones
2. Logger uses the same device for input
3. You'll hear the pile-up in your headphones

#### Option B: Virtual Audio Cable
For separate audio routing:

```bash
# Create virtual audio sink
pactl load-module module-null-sink sink_name=testsim_audio sink_properties=device.description="Contest Simulator_Audio"

# Create loopback to make it an input source
pactl load-module module-loopback sink=testsim_audio source=testsim_audio.monitor

# Tell Contest Simulator to use this sink (edit ~/.asoundrc or use pavucontrol)
```

Then configure your logger to use "Contest Simulator_Audio" as audio input.

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
  --serial <device>    Serial port for WinKeyer3 (default: /dev/ttyVK0)
  --config <file>      Configuration file (default: none)
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
./testsim --serial /tmp/testsim --config contest.ini
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

## Troubleshooting

### "Could not open serial port"

```bash
# Check port exists
ls -l /tmp/testsim

# Check permissions
# Virtual ports from socat should be user-accessible

# Verify socat is still running
ps aux | grep socat
```

### "WinKeyer not responding" in logger

1. Make sure socat is running
2. Check you're connecting to `/tmp/logger` (not `/tmp/testsim`)
3. Try different WinKeyer modes (WK2, WK3)
4. Check Contest Simulator daemon is running and connected

### "No audio" or "Can't hear pile-up"

```bash
# Check Contest Simulator is outputting audio
pactl list sink-inputs

# Verify default audio device
pactl info | grep "Default Sink"

# Test audio
speaker-test -t sine -f 440
```

### Latency Issues

If you experience audio lag:
1. Reduce buffer size in contest.ini: `bufsize=256` (default 512)
2. Close other audio applications
3. Use ALSA instead of PulseAudio for lower latency

## Advanced: Permanent Virtual Serial Ports

For persistent virtual ports across reboots:

```bash
# Install socat as a systemd service
sudo systemctl edit --force --full testsim-serial.service
```

Add:
```ini
[Unit]
Description=Contest Simulator Virtual Serial Ports
After=network.target

[Service]
ExecStart=/usr/bin/socat pty,raw,echo=0,link=/tmp/logger pty,raw,echo=0,link=/tmp/testsim
Restart=always

[Install]
WantedBy=multi-user.target
```

Enable:
```bash
sudo systemctl enable testsim-serial.service
sudo systemctl start testsim-serial.service
```

## Tips

- **Adjust activity** for realistic pile-up density
- **Enable lids** for practice with operator mistakes
- **QSB on** makes copying more challenging
- **Single mode** for practicing one-at-a-time QSOs
- **Monitor multiple loggers** by creating additional virtual port pairs

## Support

For issues or questions, see the main README.md or check the documentation in CLAUDE.md.
