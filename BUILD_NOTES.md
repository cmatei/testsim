# Build Notes

## Dependencies

Contest Simulator has **minimal dependencies**:

**Runtime Dependencies:**
- RtAudio (audio output)
- Audio backend (one of: PulseAudio, JACK, ALSA)
- Standard C/C++ runtime
- Network sockets (built-in)

**Build Dependencies:**
- CMake 3.16+
- C++17 compiler (g++ or clang++)
- RtAudio development headers

## Building

### Standard Build

```bash
cmake .
make
```

This builds the `testsim` executable.

## Verifying Dependencies

Check what libraries the daemon links against:

```bash
ldd testsim
```

Expected output:
```
librtaudio.so.7 => /usr/lib/librtaudio.so.7
libstdc++.so.6 => /usr/lib/libstdc++.so.6
libm.so.6 => /usr/lib/libm.so.6
libc.so.6 => /usr/lib/libc.so.6
... (audio backends: libjack, libpulse, libasound)
```

## Distribution-Specific Packaging

### Debian/Ubuntu Package

Minimal dependencies for daemon:
```
Depends: librtaudio7, libasound2 | libpulse0 | libjack0
```

### Fedora/RHEL Package

```
Requires: rtaudio >= 6.0, alsa-lib (or pulseaudio-libs or jack-audio-connection-kit)
```

### Arch Linux PKGBUILD

```bash
depends=('rtaudio' 'alsa-lib')
optdepends=('pulseaudio: for PulseAudio backend'
            'jack: for JACK backend')
```

## Static Linking (Optional)

To create a more portable binary:

```bash
cmake . -DCMAKE_EXE_LINKER_FLAGS="-static-libgcc -static-libstdc++"
make testsim
```

Note: You'll still need RtAudio and audio backend libraries at runtime.

## Cross-Compilation

Example for Raspberry Pi:

```bash
cmake . -DCMAKE_TOOLCHAIN_FILE=/path/to/arm-toolchain.cmake
make testsim
```

## Minimal System Requirements

- **CPU**: Any x86_64 or ARM processor (tested on x86_64)
- **RAM**: ~10MB runtime, ~50MB during contest with high activity
- **Disk**: 500KB binary + ~500KB call sign database
- **Audio**: Any ALSA/PulseAudio/JACK compatible device

## Troubleshooting

### "Could not find RtAudio"

```bash
# Debian/Ubuntu
sudo apt-get install librtaudio-dev

# Fedora
sudo dnf install rtaudio-devel

# From source
git clone https://github.com/thestk/rtaudio.git
cd rtaudio
cmake . && make && sudo make install
```

### "CMake version too old"

Minimum CMake 3.16 required. Upgrade via:
```bash
# Ubuntu 20.04+, Debian 11+ have 3.16+
sudo apt-get update && sudo apt-get install cmake

# Or use CMake from snap
sudo snap install cmake --classic
```

## Size Comparison

```
testsim (stripped):      ~300KB
testsim (unstripped):    ~434KB

Runtime memory:
  Idle:                     ~8MB
  Active contest:           ~15-50MB (depends on pile-up size)
```

## Performance Notes

- QSB/QRN/QRM add minimal overhead (~1-2% CPU each)

