# Notes & Lessons Learned

## 1. Understanding RFFT's Packed Layout

CMSIS-DSP’s Real FFT (RFFT), `arm_rfft_fast_f32()`, produces an array of `FFT_SIZE` floats — but that array is *packed* in a nonstandard way compared to a conventional complex FFT.

For a forward transform:

- The output length is **FFT_SIZE**, but only **FFT_SIZE/2 + 1 unique frequency bins** are represented. 

- The first two elements represent **DC (index 0)** and **Nyquist (index 1)**.

- The remaining data is stored as interleaved real and imaginary values for bins 1 through (FFT_SIZE / 2 − 1).

| Index     | Meaning                      | Description                                                |
|:--------- |:---------------------------- |:---------------------------------------------------------- |
| `0`       | **DC (0 Hz)**                | Real-only component — the mean of the input signal.        |
| `1`       | **Nyquist (Fs / 2)**         | Real-only component (only valid for even N).               |
| `2 … N−1` | **Interleaved complex data** | Pairs of real and imaginary components for bins 1 … N/2−1. |

The out put array of `arm_rfft_fast_f32()` has  the following layout:

```
Index:   0     1     2     3     4     5     6     7   ... 
Content: DC    Nyq   Re1   Im1   Re2   Im2   Re3   Im3  ...
```

This layout convention is unique to CMSIS-DSP’s **arm_rfft_fast_f32()** and is optimized for in-place transforms on ARM Cortex-M cores.

When integrating this into your own code, make sure buffer access patterns, bin indexing, and magnitude-phase conversions respect this structure.

---

## 2. DC and Nyquist Bins Are Real-Only

When converting to magnitude and phase, the **DC (index 0)** and **Nyquist (index 1)** bins contain *no imaginary component*.  

If you mistakenly treat them as complex or apply operations like `abs()` indiscriminately, you’ll destroy their sign — which affects waveform symmetry on inverse reconstruction.

- The **DC sign** determines the polarity (direction) of the average offset.
- The **Nyquist sign** maintains the correct mirror symmetry at the top of the spectrum.

Losing those signs can lead to subtle distortion, DC bias, or a faint “whine” in the reconstructed signal.

---

## 3. Practical Implications

- **DC (index 0)** and **Nyquist (index 1)** are *real-only* values — they don’t have corresponding imaginary components.
- Complex bins begin at index 2 (Re[1], Im[1]) and continue until `N−1`.
- When converting to magnitude and phase, skip index 0 and 1 — handle them separately.

Example magnitude-phase conversion :

```cpp
static inline void ToMagPhase(const float *fft_data,
                              float *mags,
                              float *phases,
                              size_t fft_size)
{
    size_t half = fft_size / 2; // N/2

    /** DC component (real only, first bin) */
    mags[0] = fft_data[0];
    phases[0] = 0.0f; // DC phase meaningless, keep zero

    /** Complex bins (1 .. N/2-1) */
    for (size_t k = 1; k < half; ++k)
    {
        // Get real and imaginary parts of complex bin
        float re = fft_data[2 * k];
        float im = fft_data[2 * k + 1];
        mags[k] = sqrtf(re * re + im * im);
        phases[k] = atan2f(im, re);
    }

    /** Nyquist component (real only, last bin) */
    mags[half] = fft_data[1];
    phases[half] = 0.0f; // Nyquist phase also undefined
}
```

---

## 4. What DC Actually Represents

Mathematically, the DC component is the **mean value** of the time-domain signal:

$$
X[0] = \sum_{n=0}^{N-1} x[n]

$$

For audio signals, DC is inaudible — it’s a constant offset — but it still matters:

- Persistent DC shifts the waveform up or down.
- It can cause rumble or slow bias drift when modulated.
- In extreme cases, it eats headroom and causes clipping.

So, while you won’t *hear* DC, you’ll definitely *feel* it in the mix if it’s not handled properly.

---

## 5. Should You Zero the DC Component?

That depends on your use case:

| Use Case                             | Recommendation | Rationale                                        |
| ------------------------------------ | -------------- | ------------------------------------------------ |
| **Audio (music, effects)**           | ✅ Zero it      | Keeps the waveform centered and prevents rumble. |
| **Analytical FFT (scientific data)** | ⚠️ Keep it     | The mean value may be meaningful.                |
| **Control or modulation signals**    | ❌ Depends      | Preserve if encoding an intentional bias.        |

**Rule of thumb for audio:**  
When performing operations like *spectral gating*, *EQ*, *blurring*, or *reverb*, always **zero or skip DC** during processing, then reconstruct without it.

---

## 6. Treating DC and Nyquist During Spectral Effects

When implementing spectral operations (e.g., *spectral blur*), handle DC and Nyquist explicitly:

### DC (k = 0)

- Represents the *average value* (not a tone).
- Including it in a blur-like effect spreads that offset into nearby low frequencies.
- Excluding prevents LF rumble or bias.
- **Exclude or zero it**.

### Nyquist (k = FFT_SIZE/2)

- Represents the *cosine component at half the sample rate (Fs/2)*.
- Has no imaginary part, and no bin beyond it.
- **Leave unchanged**; can break symmetry if modified

---

## 7. Final Thoughts on RFFT

The main takeaway from working with CMSIS-DSP’s RFFT is that **it does exactly what it should**, but you must respect the assumptions it makes:

- **DC and Nyquist are real-valued.**
- **Only N/2 + 1 bins are unique.**
- **Phase symmetry matters.**
- **A careless `fabsf()` can wreck your waveform.**

When implementing custom spectral effects or visualizations, always remember:

- **Handle DC and Nyquist separately.**
- **Don’t attempt to read imaginary components for bins 0 or 1.**
- **When converting back to time domain, reconstruct the packed array exactly in this order.**

---

## 8. Other Matters

### Frequency of Each FFT Bin

Each FFT bin corresponds to a **center frequency** given by:

$$
f_{bin}​(k)= \frac{k⋅f_{s}​​}{N}
$$

where:

- k = bin index (0 → N/2 for real FFT)

- f_{s} = sampling rate (Hz)

- N = FFT size

For example, with FFT size of 1024 and sample rate of 48,000Hz. each bin has an incremental frequency of 46.875Hz as calculated in the following:

$$
Δf= \frac{fs}{N}= \frac{48000}{1024}≈46.875

$$

### Spectral delay + windowing

- Hann window ensures **COLA (Constant OverLap-Add)** when `hop = FFT/4` for perfect reconstruction.
- Linear-mean COLA normalization avoids “buzz” artifacts caused by RMS normalization.

### Complex-domain processing

- For linear, phase-sensitive operations (filters, convolution, etc.), skipping `ToMagPhase` / `FromMagPhase` saves CPU and maintains exact phase relationships.
- Compile-time mode selection removes unused code completely.

### Performance tuning

- If CPU load is high, consider reducing FFT size or hop overlap (larger hop size).
- Choose `HOP_SIZE = FFT / 4` for COLA-compliant overlap.
- Keep `BLOCK_SIZE` small (64 samples @ 48kHz ≈ 1.3ms).
- Use **complex mode** to skip mag/phase conversions if CPU is tight.

### Debugging spectral effects

- Silence often means `RunFrame()` never triggers — ensure `accum_ >= HOP_SIZE`.
- Always clear overlap buffers between blocks.
- Verify COLA gain normalization (should be near `1.0 / sum(w²)`).

### Scaling / RMS checks

- You can use `arm_rms_f32()` for adaptive normalization or dynamic gain tracking.

---

## 