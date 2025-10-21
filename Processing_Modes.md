# Processing Mode Comparison: MagPhase vs. Complex

This document explains when and why to use each spectral processing mode in the `dsp::Fast_STFT`. It covers conceptual differences, design tradeoffs, and practical examples for DSP applications on the Daisy Seed.

---

## Core Conceptual Difference

| Aspect               | **MagPhase Mode**                                    | **Complex Mode**                                      |
| -------------------- | ---------------------------------------------------- | ----------------------------------------------------- |
| Data format          | `\|X[k]\|` (magnitude) and `φ[k]` (phase)            | `Re(X[k])` (real) and `Im(X[k])` (imaginary)          |
| Operation domain     | Polar form (intuitive, perceptual)                   | Rectangular form (mathematically direct)              |
| Typical manipulation | Scale magnitudes, alter or offset phases             | Multiply, filter, or convolve directly                |
| Phase continuity     | Must be managed manually                             | Preserved automatically via complex math              |
| CPU load             | High (trig operations per bin)                       | Low (simple multiply/add per bin)                     |
| Best suited for      | Sound design, analysis, EQ, morphing, phase vocoding | Filtering, convolution, modulation, physical modeling |

---

## When to Use **`ProcessingMode::MagPhase`**

This mode makes sense when your algorithm benefits from separating **"what is sounding"** (magnitude) from **"when and how it's sounding"** (phase).

### Use `MagPhase` for:

#### **A. Spectral EQ and Filtering**

Modify amplitudes per bin for perceptual spectral shaping.

```cpp
mags[k] *= myEQCurve[k];
```

#### **B. Spectral Compression / Expansion**

Apply per-bin gating or dynamic control for noise reduction or spectral enhancement.

#### **C. Cross-Synthesis & Timbre Morphing**

Combine magnitude spectra from two signals while keeping phase from one — classic vocoder effects.

#### **D. Pitch Shifting / Time Stretching**

Manipulate inter-frame phase evolution for phase vocoder time scaling or pitch scaling.

#### **E. Spectral Delay / Reverb / Motion**

Adjust phase per frequency to create time delays or stereo widening.

#### **F. Visualization / Analysis**

Compute spectral magnitude for spectrum analyzers or feature extraction (centroid, rolloff, etc.).

#### **G. Sound Design & Nonlinear Effects**

Create artistic transformations — spectral blur, shimmer, freeze, or morphing.

---

### Avoid `MagPhase` when:

- Implementing linear filters or convolution (unnecessary conversions).  
- Working on CPU-constrained systems with large FFTs.  
- You require perfect phase coherence or linear-phase response.

---

## When to Use **`ProcessingMode::Complex`**

This mode is ideal for mathematically linear operations — anything that can be expressed as multiplication or convolution in the frequency domain.

### Use `Complex` for:

#### **A. Convolution and FIR Filtering**

Frequency-domain convolution = elementwise complex multiplication.

```cpp
fft_out[k] *= H[k];
```

#### **B. Linear Phase or All-Pass Filters**

Apply frequency-dependent phase rotation directly in the complex domain.

#### **C. Spectral Delay or Modulation with Phase Coherence**

Rotate or shift complex bins without breaking phase relationships.

#### **D. Phase-Coherent Stereo or Multi-Channel Processing**

Preserves phase alignment between channels for true stereo imaging.

#### **E. Performance-Critical DSP**

Avoids trig and sqrt operations, ~2× faster for same FFT size on ARM Cortex-M7.

#### **F. Analytic or IQ (Quadrature) Processing**

Used for AM/FM modulation, demodulation, or Hilbert-based analytic signals.

---

### Avoid `Complex` when:

- You want perceptual, amplitude-based control (EQ-like effects).  
- You need explicit access to magnitude or phase for analysis.  
- Your focus is on sound shaping rather than mathematical filtering.

---

## Performance Comparison (Cortex-M7, 480 MHz)

| Parameter           | MagPhase                   | Complex                        |
| ------------------- | -------------------------- | ------------------------------ |
| Operations per bin  | 2× trig + 1× sqrt + 1× mul | 1× mul or add                  |
| CPU load (FFT=1024) | ~25–30% higher             | Baseline                       |
| Memory usage        | +2× arrays (mag + phase)   | Baseline (complex buffer only) |
| Numerical stability | Slightly lower             | Excellent                      |

**Tip:** Prototype creative effects in MagPhase, then optimize to Complex for deployment.

---

## Design Summary

| Attribute          | **MagPhase Mode**         | **Complex Mode**                   |
| ------------------ | ------------------------- | ---------------------------------- |
| Domain             | Perceptual                | Mathematical                       |
| Best For           | EQ, timbre, pitch/time FX | Filtering, convolution, modulation |
| CPU Cost           | Higher                    | Lower                              |
| Ease of Use        | Intuitive                 | Requires complex math literacy     |
| Phase Handling     | Manual                    | Implicit                           |
| Sound Design Power | High                      | Moderate                           |
| Latency            | Slightly higher           | Minimal                            |

---

## Practical Guidelines

| Goal                               | Recommended Mode | Rationale                                 |
| ---------------------------------- | ---------------- | ----------------------------------------- |
| Real-time EQ / spectral compressor | MagPhase         | Perceptual control over amplitude         |
| Convolution reverb                 | Complex          | Efficient frequency-domain multiplication |
| Phase vocoder / time stretch       | MagPhase         | Explicit phase tracking                   |
| Spectral delay / shimmer           | MagPhase         | Frequency-dependent phase modulation      |
| Long FIR filter                    | Complex          | Linear convolution                        |
| Morphing or hybrid synthesis       | MagPhase         | Blend magnitudes across sources           |
| IQ modulation / AM-FM synthesis    | Complex          | Works directly with complex signals       |
| CPU-limited embedded DSP           | Complex          | Avoids expensive trig ops                 |
| Visualization / spectral analysis  | MagPhase         | Uses magnitudes directly                  |

---

## Summary Table

| If you think like this...                                      | Use this mode |
| -------------------------------------------------------------- | ------------- |
| “I want to shape or understand how it *sounds*.”               | MagPhase      |
| “I want to precisely control how it *behaves* mathematically.” | Complex       |

---

## Useage Tip

- **Prototype in MagPhase** for clarity and creativity.  
- **Deploy in Complex** for real-time efficiency and phase stability.

Many spectral effects (EQs, morphers, spatializers) start as MagPhase algorithms and can later be reimplemented in Complex form once their behavior is well-understood.

---
