# FFT/STFT Parameter Guide: Recommended Settings for Real-Time Spectral Effects

This document provides practical parameter pairings for the `Fast_STFT` framework — optimized for 48 kHz audio and the Daisy Seed (ARM Cortex-M7 @ 480 MHz).

---

## 1. General Guidelines

| Parameter      | Effect                                            | Notes                                                         |
| -------------- | ------------------------------------------------- | ------------------------------------------------------------- |
| **FFT_SIZE**   | Controls frequency resolution and overall latency | Larger FFT → finer spectral detail, higher latency            |
| **HOP_SIZE**   | Controls overlap and update rate                  | Smaller hop → smoother output, higher CPU                     |
| **BLOCK_SIZE** | Controls audio callback chunk size                | Should divide `HOP_SIZE`; smaller block → smoother modulation |

**Latency ≈ (FFT_SIZE / 2) / 48 000 s**  
**CPU load ∝ (FFT_SIZE / HOP_SIZE)**

---

## 2. Recommended Configurations by Effect Type

| Effect Type                                    | FFT_SIZE | HOP_SIZE | Overlap | BLOCK_SIZE | Characteristics                            | Comments                              |
| ---------------------------------------------- | -------- | -------- | ------- | ---------- | ------------------------------------------ | ------------------------------------- |
| **Spectral EQ / Filtering**                    | 512      | 128      | 4×      | 64         | Moderate latency (~5 ms), smooth response  | Balanced for tone shaping; low CPU    |
| **Spectral Delay / Reverb**                    | 1024     | 256      | 4×      | 64–128     | Lush frequency smearing (~10.7 ms latency) | Great for space and width             |
| **Vocoder / Spectral Morphing**                | 2048     | 256      | 8×      | 64         | High resolution (~21 ms latency)           | Needed for precise formant control    |
| **Pitch-Shift / Time-Stretch (Phase Vocoder)** | 2048     | 128      | 16×     | 32         | Smooth pitch evolution; heavy CPU          | Long window stabilizes phase tracking |
| **Transient Shaper / Flux Detection**          | 512      | 64       | 8×      | 32         | Fast response (~5 ms latency)              | Good temporal precision               |
| **Granular / Spectral Freeze**                 | 1024     | 128      | 8×      | 32         | Smooth spectral blending                   | Ideal for frozen textures             |
| **Spectral Modulation / Feedback FX**          | 1024     | 256      | 4×      | 64         | Balanced detail & smoothness               | Organic-sounding results              |
| **Feature Extraction (centroid, flux, etc.)**  | 512      | 128      | 4×      | 64         | Low CPU, stable analysis                   | Good baseline analytical setup        |

---

## 3. CPU Load and Latency Estimates (Daisy Seed @ 480 MHz)

| FFT_SIZE | HOP_SIZE | Overlap | CPU Usage | Latency (ms) |
| -------- | -------- | ------- | --------- | ------------ |
| 512      | 128      | 4×      | ~3 %      | 5.3          |
| 1024     | 256      | 4×      | ~5 %      | 10.7         |
| 1024     | 128      | 8×      | ~7 %      | 10.7         |
| 2048     | 256      | 8×      | ~9 %      | 21.3         |
| 2048     | 128      | 16×     | ~13 %     | 21.3         |

*Assumes Mag/Phase mode with a light processing callback.*

---

## 4. Practical Tips

1. **Use power-of-two sizes.**  
   Circular bitmasking (`FFT_MASK`, etc.) depends on this.
2. **Keep `BLOCK_SIZE ≤ HOP_SIZE / 4`.**  
   Prevents buffer underruns and improves responsiveness.
3. **Tune overlap before FFT size.**  
   Increasing overlap improves smoothness more efficiently than increasing FFT size.
4. **Smaller FFTs** → better transient accuracy.  
   **Larger FFTs** → better harmonic precision.
5. **Measure on-device CPU** (`dsy_system_getnow()` or Daisy profiler) before finalizing parameters.

---

## 5. Example “Profiles”

| Profile                       | Purpose                                   | Parameters                        |
| ----------------------------- | ----------------------------------------- | --------------------------------- |
| **Low-latency live FX**       | EQ, filtering, envelope modulation        | `FFT=512`, `HOP=128`, `BLOCK=64`  |
| **High-fidelity spectral FX** | Morphing, spectral freeze, harmonizer     | `FFT=1024`, `HOP=256`, `BLOCK=64` |
| **Precision vocoder / pitch** | Phase vocoder, vocoding, formant tracking | `FFT=2048`, `HOP=128`, `BLOCK=32` |
| **Analysis & features**       | Spectral flux, centroid, spectral balance | `FFT=512`, `HOP=128`, `BLOCK=64`  |

---

## In Summary

- **For musical DSP:** `FFT=1024`, `HOP=256`, `BLOCK=64` — balanced and efficient.  
- **For reactive/transient effects:** `FFT=512`, `HOP=128`, `BLOCK=32`.  
- **For rich evolving spectral work:** `FFT=2048`, `HOP=256`, `BLOCK=64`.  

These settings keep CPU usage under 10% on Daisy Seed while providing professional-grade STFT performance suitable for real-time spectral effects, vocoders, and analysis tools.
