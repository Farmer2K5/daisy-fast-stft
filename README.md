# Real-Time FFT/STFT Framework for Embedded Spectral Audio Processing

This modular C++ framework provides a unified interface for **FFT/STFT/ISTFT**, **windowing**, **spectral conversions**, **frequency-domain operations**, and **spectral feature extraction** — all designed for for _high-performance real-time_ spectral audio processing on the [**Electrosmith Daisy Seed**](https://www.electro-smith.com/daisy/daisy).

## ⚠️ Work in Progress Disclaimer

This is an experimental project and a work in progress.

It was developed primarily for research and learning on the Daisy Seed, and while it has been tested on real hardware, no guarantees are made regarding the accuracy, stability, or sonic quality of its results.

This code should be viewed as a foundation for exploration — not a finished product. You are encouraged to modify, measure, and validate it for your own purposes, and to share improvements or findings with the community.

## Highlights

- **Designed for the Daisy Seed** (Cortex-M7 @ 480 MHz Boost Mode)
- **Real-time FFT/STFT/ISTFT** spectral processing using CMSIS-DSP
- **Header-only**, self-contained, and lightweight — no heap allocation
- **COLA-correct overlap-add** synthesis for perfect reconstruction
- **Spectral conversions and analysis** for packed FFT data
- **Feature extraction**: centroid, flux, rolloff, spread, flatness, and more
- Clean integration with **CMSIS-DSP (`arm_math.h`)**

## Module Overview

```
┌──────────────────────────┐
│         FFTStack         │
│ ┌──────────────┐         │  Core → FFT, STFT, ISTFT, Windowing
│ │   Core DSP   │         │
│ └──────────────┘         │
│ ┌──────────────┐         │  Spectral → Conversions, Ops, Features
│ │   Spectral   │         │
│ └──────────────┘         │
└──────────────────────────┘
```

### Core Modules

- `fast_rfft.h` — CMSIS-DSP real FFT wrapper (`Fast_RFFT`)
- `fast_stft.h` — Streaming Short-Time Fourier Transform (`Fast_STFT`)
- `fast_istft.h` — Inverse STFT for overlap-add synthesis (`Fast_ISTFT`)
- `fast_window.h` — Window generation and normalization

### Spectral Modules

- `fast_spectral.h` — FFT ↔ magnitude/phase conversions and frequency bins
- `fast_spectral_ops.h` — Frequency-domain arithmetic and morphing tools
- `fast_spectral_features.h` — Spectral feature extraction utilities

Include all modules at once:

```cpp
#include "fast_dsp.h"
```

### STFT Module

The **STFT Module** provides a block-based pipeline that handles:

- Circular buffering and overlap-add reconstruction
- Hann windowing for perfect COLA (Constant Overlap-Add) synthesis
- Fast real FFT (`Fast_RFFT`) via [CMSIS-DSP](https://arm-software.github.io/CMSIS_5/DSP/html/index.html)
- Optional magnitude/phase conversion
- User-overridable spectral processing callbacks

The STFT module supports **two compile-time modes**:

- `ProcessingMode::MagPhase` — operates on magnitude and phase arrays (perceptual domain)
- `ProcessingMode::Complex` — operates directly on FFT complex bins (mathematical domain)

See [`Processing_Modes.md`](./Processing_Modes.md) for futher details.

## Practical Examples

### 1️⃣ Real-Time Spectral Effect

```cpp
#include "fast_dsp.h"
using namespace daisyfarm;

// Simple high-frequency damping effect
class HighCut : public Fast_STFT<1024, 256, 64, ProcessingMode::MagPhase> {
public:
    void ProcessFrame(float* mags, float* phases, size_t n_bins) override {
        const float fs = 48000.0f;
        const float bin_hz = fs / FFT_SIZE;
        for (size_t k = 0; k < n_bins; ++k) {
            if (bin_hz * k > 8000.0f) mags[k] *= 0.4f; // attenuate highs
        }
    }
};

HighCut stft;

void AudioCallback(float** in, float** out, size_t size) {
    stft.ProcessAudioBlock(in[0], out[0]);
}
```

**Concept:** Subclass `Fast_STFT` to process magnitude and phase (perceptual domain).  
**Result:** Smooth high-frequency roll-off with natural reconstruction.

### 2️⃣ Complex-Domain Phase Modulator

```cpp
class PhaseTilt : public Fast_STFT<1024, 256, 64, ProcessingMode::Complex> {
public:
    void ProcessFrameComplex(float* fft_bins, size_t n_bins) override {
        for (size_t k = 1; k < n_bins; ++k) {
            float re = fft_bins[2*k];
            float im = fft_bins[2*k + 1];
            float p  = 0.002f * k;
            float c = arm_cos_f32(p);
            float s = arm_sin_f32(p);
            fft_bins[2*k]     = re*c - im*s;
            fft_bins[2*k + 1] = re*s + im*c;
        }
    }
};
```

**Concept:** Operates directly on complex bins to apply progressive phase skew.  
**Result:** Produces a “warbling” or “bent phase” spectral effect.

### 3️⃣ Offline Resynthesis with ISTFT

```cpp
#include "fast_dsp.h"

constexpr size_t FFT_SIZE = 1024;
constexpr size_t HOP_SIZE = 256;

daisyfarm::Fast_ISTFT<FFT_SIZE, HOP_SIZE> istft;

void ReconstructFromFrames(float** fft_frames, size_t num_frames, float* output) {
    float hop[HOP_SIZE];
    size_t pos = 0;
    for (size_t i = 0; i < num_frames; ++i) {
        istft.ProcessFrame(fft_frames[i], hop);
        memcpy(&output[pos], hop, sizeof(float) * HOP_SIZE);
        pos += HOP_SIZE;
    }
}
```

**Concept:** Demonstrates frame-based reconstruction from spectral data.  
**Use Case:** Ideal for resynthesis after spectral modification or machine-learning processing.

### 4️⃣ Spectral Analysis and Feature Extraction

```cpp
#include "fast_dsp.h"

constexpr size_t FFT_SIZE = 1024;
constexpr size_t N_BINS = FFT_SIZE / 2 + 1;

float mags[N_BINS];
float freqs[N_BINS];
float prev_mags[N_BINS];

daisyfarm::ComputeFrequencyBins(freqs, FFT_SIZE, 48000.0f);

float centroid = daisyfarm::spectral::features::SpectralCentroid(mags, freqs, N_BINS);
float flux     = daisyfarm::spectral::features::SpectralFlux(mags, prev_mags, N_BINS);
float flatness = daisyfarm::spectral::features::SpectralFlatness(mags, N_BINS);
```

**Concept:** Compute scalar spectral descriptors for feature tracking or visualization.  
**Result:** Produces normalized analysis metrics useful for adaptive DSP or ML inputs.

### 5️⃣ Spectral Morphing Using Ops

```cpp
#include "fast_dsp.h"

constexpr size_t FFT_SIZE = 1024;
constexpr size_t N_BINS = FFT_SIZE / 2 + 1;

float fftA[FFT_SIZE]; // Spectrum A
float fftB[FFT_SIZE]; // Spectrum B
float mixed[FFT_SIZE];

daisyfarm::WeightedMix(fftA, fftB, mixed, 0.5f, FFT_SIZE);
```

**Concept:** Blends two FFT frames’ magnitude and phase smoothly.  
**Result:** Enables cross-synthesis or morphing between sound sources.

## Performance

Typical throughput and profiling data are summarized in [Performance.md](Performance.md).

## Developer Notes

Design rationale, CMSIS-DSP nuances, and FFT behavior insights are provided in [Notes.md](Notes.md).

## Dependencies

- [Electrosmith DaisySP](https://github.com/electro-smith/DaisySP)
- [CMSIS-DSP](https://arm-software.github.io/CMSIS_5/DSP/html/index.html)
- ARM Cortex-M toolchain (GCC or clang)

## License & Acknowledgments

This framework is released under a permissive open license.  
You are free to use, modify, and redistribute this work, with attribution appreciated to the embedded DSP community.

> None of this code is uniquely mine — it draws heavily on the shared knowledge of the **Daisy**, **Teensy**, **ARM**, and other DSP communities.  
> I am not a professional C++ developer, and this framework represents an ongoing learning effort built from their contributions.

Special thanks to the countless developers, forum members, and DSP enthusiasts whose posts, code snippets, and shared experiments continue to inspire.
