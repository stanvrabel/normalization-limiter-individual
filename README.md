# normalization-limiter-individual

64-bit VST3 limiter plugin in C++ for Visual Studio, using an improved rhythm/harmonics release algorithm with an emergency safety stage.

## Features
- Stereo linked limiting
- ~5 ms lookahead (reported latency)
- Rhythm/Harmonics blend:
  - Rhythm = faster release
  - Harmonics = slower release
- Emergency ceiling safety stage
- Sample-peak limiting (not true-peak)

## Requirements
- Windows x64
- Visual Studio 2022 (MSVC v143)
- Steinberg VST3 SDK (https://github.com/steinbergmedia/vst3sdk)
- CMake 3.20+

## Build
1. Clone this repository.
2. Set environment variable `VST3_SDK_ROOT` to your local vst3sdk path.
3. Configure and build:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

If `VST3_SDK_ROOT` is not set, CMake also checks `../vst3sdk`.

## Output
The VST3 bundle is produced by the VST3 SDK CMake macros in the build output directory.

## Parameters
- **Input Gain**: -12 dB .. +24 dB
- **Ceiling**: -24 dB .. 0 dB (default -0.3 dB)
- **Release**: 10 ms .. 1000 ms
- **Rhythm<>Harmonics**: 0..1 blend

## Notes
- This is sample-peak limiting. For true-peak behavior, add oversampling in detection/output stages.
