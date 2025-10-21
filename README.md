# STFT Framework for Daisy Seed

A modular **Short-Time Fourier Transform (STFT)** framework for real-time **spectral audio processing** on the [Daisy Seed](https://www.electro-smith.com/daisy).  The framework enables efficient frequency-domain effects, analysis, and transformations using a simple, extensible C++ API.

---

## Overview

The **STFT Framework** provides a block-based pipeline that handles:

- Circular buffering and overlap-add reconstruction  
- Hann windowing for perfect COLA (Constant Overlap-Add) synthesis  
- Fast real FFT (`Fast_RFFT`) via [CMSIS-DSP](https://arm-software.github.io/CMSIS_5/DSP/html/index.html)  
- Optional magnitude/phase conversion  
- User-overridable spectral processing callbacks  

The framework supports **two compile-time modes**:

- `ProcessingMode::MagPhase` — operates on magnitude and phase arrays (perceptual domain)  
- `ProcessingMode::Complex` — operates directly on FFT complex bins (mathematical domain)

See [`Notes.md`](./Notes.md) for Insights on CMSIS-DSP RFFT behavior, DC/Nyquist handling, and implementation notes.

---

## Architecture

```
Audio Input (ADC / I²S / Daisy Audio Callback)
        │
        ▼
Circular Buffer Manager
    • Collects streaming samples
    • Triggers STFT frame when hop size reached
        │
        ▼
Windowing Stage
    • Applies Hann window (COLA-compliant)
    • Computes COLA normalization gain
        │
        ▼
Forward FFT (arm_rfft_fast_f32)
    • Converts time-domain frame → frequency-domain bins
    • Produces N/2+1 complex bins
        │
        ▼
───────────────────────────────────────────────────────────────────
 USER PROCESSING STAGE
───────────────────────────────────────────────────────────────────
    MagPhase Mode:
        - ToMagPhase()
        - ProcessFrame()
        - FromMagPhase()

    Complex Mode:
        - ProcessFrameComplex()

    (Apply EQ, spectral delay, vocoder, morphing, filtering, etc.)
───────────────────────────────────────────────────────────────────
        │
        ▼
Inverse FFT (arm_rfft_fast_f32)
    • Converts frequency-domain bins → time-domain frame
        │
        ▼
Overlap-Add Synthesis
    • Combines overlapping frames
    • Ensures perfect reconstruction (COLA)
        │
        ▼
Audio Output (DAC / I²S / Daisy Audio Callback)
```

---

## Components

### 1. `dsp::Fast_RFFT<FFT_SIZE>`

**Purpose:**  
A lightweight, type-safe C++ wrapper around the CMSIS-DSP *fast real FFT* (`arm_rfft_fast_f32`) routines. It provides a minimal interface for forward/inverse transforms, plus optional utilities for:

- Generating and applying windows (Hann, Hamming, Blackman, Gaussian).
- Normalizing windows (Sum or RMS).
- Converting between complex FFT output and magnitude/phase arrays.

**Key points:**

- Works entirely in `float32`.
- Copyable (no heap allocation or static state).
- Supports FFT sizes 32 → 4096 samples.

**Primary API:**

```cpp
dsp::Fast_RFFT<1024> fft;

fft.Forward(time_buf, freq_buf);   // Real -> complex
fft.Inverse(freq_buf, time_buf);   // Complex -> real

fft.ToMagPhase(freq_buf, mags, phases);
fft.FromMagPhase(mags, phases, freq_buf);
```

### 2. `dsp::Fast_STFT<FFT_SIZE, HOP_SIZE, BLOCK_SIZE, PROCESSING_MODE>`

**Purpose:**  
Implements a full **streaming STFT engine** for real-time DSP — an overlap-add framework that:

1. Buffers audio blocks.
2. Applies a window.
3. Runs the FFT via `Fast_RFFT`.
4. Lets the user modify the spectrum.
5. Reconstructs the signal via inverse FFT and COLA synthesis.

**Modes (compile-time via template):**

- `ProcessingMode::MagPhase` — Convert to magnitude/phase, modify, and rebuild.
- `ProcessingMode::Complex` — Skip polar conversion and process complex bins directly (for convolution, all-pass, etc.).

**You subclass it and override** one of:

```cpp
void ProcessFrame(float* mags, float* phases, size_t n_bins) override;
```

or

```cpp
void ProcessFrameComplex(float* fft_bins, size_t n_bins) override;
```

**Example: simple magnitude scaling**

```cpp
class MySpectralGain : public dsp::Fast_STFT<1024, 256, 64> {
public:
    void ProcessFrame(float* mags, float* phases, size_t n_bins) override {
        for (size_t i = 0; i < n_bins; ++i)
            mags[i] *= 0.5f;  // attenuate by 6 dB
    }
};
```

---

## Usage Example

### Basic setup:

```cpp
#include "fast_stft.h"
using namespace dsp;

class MySpectralEQ : public Fast_STFT<1024, 256, 64, ProcessingMode::MagPhase>
{
public:
    void ProcessFrame(float* mags, float* phases, size_t n_bins) override
    {
        for (size_t k = 0; k < n_bins; ++k)
        {
            float freq = (48000.0f / 1024.0f) * k;
            if (freq > 8000.0f) mags[k] *= 0.5f; // simple high cut
        }
    }
};
```

### Daisy audio callback:

```cpp
MySpectralEQ stft;

void AudioCallback(float** in, float** out, size_t size)
{
    stft.ProcessAudioBlock(in[0], out[0]);
}

int main(void)
{
    hw.Init();
    hw.SetAudioBlockSize(64);
    hw.StartAudio(AudioCallback);
    while (1) {}
}
```

---

## Choosing a Processing Mode

| Mode           | Use Case                                          | Notes                                          |
| -------------- | ------------------------------------------------- | ---------------------------------------------- |
| **`MagPhase`** | EQ, spectral compression, morphing, pitch/time FX | Operates in magnitude/phase domain (intuitive) |
| **`Complex`**  | Convolution, filters, linear spectral math        | Faster, phase-coherent                         |

See [`Processing_Modes.md`](./Processing_Modes.md) for detailed comparison.

---

## Performance Notes

- Designed for **ARM Cortex-M7** targets (Daisy Seed, Daisy Patch, etc.)  
- Tested up to 2048-point FFT with 4× overlap at 48 kHz sample rate  
- Efficient fixed-size arrays, no heap allocation  
- Uses `arm_rfft_fast_f32()` for optimized FFT computation  

See [`Performance.md`](./Performance.md) for CPU load, flash usage, and optimization results on Daisy Seed

---

## Example Projects

| File              | Description                                     |
| ----------------- | ----------------------------------------------- |
| `main_mono.cpp`   | Single-channel example with CPU load monitoring |
| `main_stereo.cpp` | Dual-channel stereo processing example          |

---

## Dependencies

- [Electrosmith DaisySP](https://github.com/electro-smith/DaisySP)
- [CMSIS-DSP](https://arm-software.github.io/CMSIS_5/DSP/html/index.html)
- ARM Cortex-M toolchain (GCC or clang)

---

## License & Acknowledgments

This project is shared freely for educational and experimental purposes.  

None of the source code or documentation is uniquely mine — it builds upon the collective work, examples, and shared knowledge of the **Teensy**, **Daisy**, **Arm CMSIS-DSP** , and **other open-source communities**, whose collaboration and creativity make this kind of exploration possible.

I’m not a programmer by trade — especially not in C++ — and even less about DSP. Instead, I am someone learning through experimentation and curiosity.  This framework grew out of many hours reading forum posts, testing ideas, and learning from those far more skilled in this subject matter than I am.

You are welcome to **use, modify, copy, and distribute** any part of this work — with or without attribution — for personal, academic, or commercial use. If you find it useful, please pay it forward by contributing your own discoveries back to the community.

Special thanks to the countless developers, forum members, and DSP enthusiasts whose posts, code snippets, and shared experiments continue to make embedded audio a joy to explore.

---
