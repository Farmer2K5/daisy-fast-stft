/**
 * @file fast_spectral_ops.h
 * @brief Frequency-domain operations and utilities for manipulating RFFT spectra.
 *
 * @details
 * The **Fast Spectral Ops** module provides a collection of runtime utilities
 * for performing arithmetic, filtering, and normalization on packed
 * real FFT (RFFT) data in interleaved complex form.
 *
 * These functions form the **spectral manipulation layer** of the FastDSP framework,
 * sitting between basic spectral conversion (`fast_spectral.h`) and higher-level
 * analysis (`fast_spectral_features.h`).
 *
 * Each function assumes FFT buffers are in **CMSIS-DSP packed format**:
 *
 * | Index | Meaning | Description |
 * |:------|:---------|:-------------|
 * | 0     | DC component (real-only) | `X[0]` |
 * | 1     | Nyquist component (real-only) | `X[N/2]` |
 * | 2*k   | Real part of bin *k* | 1 ≤ k ≤ N/2 - 1 |
 * | 2*k+1 | Imag part of bin *k* | 1 ≤ k ≤ N/2 - 1 |
 *
 * Typical use cases include:
 * - Frequency-domain filtering and equalization
 * - Spectral morphing and interpolation
 * - Coherence, phase, and energy normalization
 * - Texture and freeze effects
 *
 * @see dsp::spectral for conversion utilities (`fast_spectral.h`)
 * @see dsp::spectral::features for analysis utilities (`fast_spectral_features.h`)
 *
 * @ingroup FastDSPSpectral
 * @defgroup FastDSPSpectralOps Frequency-Domain Operations
 * @brief Manipulation and filtering utilities for FFT spectra.
 */

#pragma once
#include <cmath>
#include <cstddef>
#include "arm_math.h"

namespace dsp::spectral::ops
{

    // -----------------------------------------------------------------------------
    // 1. Complex Multiplication Utilities
    // -----------------------------------------------------------------------------

    /**
     * @brief Multiply two packed RFFT spectra elementwise: `out = a * b`.
     *
     * @param[in]  a         Pointer to first packed RFFT buffer (length = `fft_size`).
     * @param[in]  b         Pointer to second packed RFFT buffer (length = `fft_size`).
     * @param[out] out       Output buffer (length = `fft_size`).
     * @param[in]  fft_size  FFT size (number of real samples in time domain).
     *
     * @details
     * Performs complex multiplication of each FFT bin:
     * \f[
     * X'[k] = A[k] \cdot B[k]
     * \f]
     *
     * Useful for:
     * - Frequency-domain convolution (e.g., reverb, FIR filters)
     * - Spectral modulation or multiplication of spectra
     *
     * @note
     * The first two elements (DC and Nyquist bins) are real-only and are multiplied directly.
     */
    inline void Multiply(const float *a, const float *b, float *out, size_t fft_size)
    {
        const size_t half = fft_size / 2;
        out[0] = a[0] * b[0]; // DC
        out[1] = a[1] * b[1]; // Nyquist

        for (size_t k = 1; k < half; ++k)
        {
            const float ar = a[2 * k];
            const float ai = a[2 * k + 1];
            const float br = b[2 * k];
            const float bi = b[2 * k + 1];

            out[2 * k] = ar * br - ai * bi;     // Real part
            out[2 * k + 1] = ar * bi + ai * br; // Imag part
        }
    }

    /**
     * @brief Cross-multiply two spectra: `out = a * conj(b)`.
     *
     * @param[in]  a         Pointer to first packed RFFT buffer (length = `fft_size`).
     * @param[in]  b         Pointer to second packed RFFT buffer (length = `fft_size`).
     * @param[out] out       Output buffer (length = `fft_size`).
     * @param[in]  fft_size  FFT size.
     *
     * @details
     * Performs complex multiplication of `a[k]` with the *complex conjugate* of `b[k]`:
     * \f[
     * X'[k] = A[k] \cdot B^*[k]
     * \f]
     *
     * Commonly used for:
     * - Spectral correlation and phase difference analysis
     * - Cross-spectral estimation
     * - Coherence computation
     */
    inline void CrossMultiply(const float *a, const float *b, float *out, size_t fft_size)
    {
        const size_t half = fft_size / 2;
        out[0] = a[0] * b[0];
        out[1] = a[1] * b[1];

        for (size_t k = 1; k < half; ++k)
        {
            const float ar = a[2 * k];
            const float ai = a[2 * k + 1];
            const float br = b[2 * k];
            const float bi = b[2 * k + 1];

            out[2 * k] = ar * br + ai * bi;     // Re{A * conj(B)}
            out[2 * k + 1] = ai * br - ar * bi; // Im{A * conj(B)}
        }
    }

    /**
     * @brief Weighted linear interpolation between two spectra: `out = (1 - mix)*a + mix*b`.
     *
     * @param[in]  a         Pointer to first packed RFFT buffer (length = `fft_size`).
     * @param[in]  b         Pointer to second packed RFFT buffer (length = `fft_size`).
     * @param[out] out       Output buffer (length = `fft_size`).
     * @param[in]  mix       Interpolation factor in [0, 1].
     * @param[in]  fft_size  FFT size.
     *
     * @details
     * Produces a smooth crossfade between two spectra, preserving both magnitude and phase.
     * Useful for:
     * - Morphing between two spectral frames
     * - Cross-synthesis
     * - Transition smoothing in spectral effects
     */
    inline void WeightedMix(const float *a, const float *b, float *out, float mix, size_t fft_size)
    {
        const size_t half = fft_size / 2;
        const float invMix = 1.0f - mix;
        out[0] = invMix * a[0] + mix * b[0];
        out[1] = invMix * a[1] + mix * b[1];

        for (size_t k = 1; k < half; ++k)
        {
            out[2 * k] = invMix * a[2 * k] + mix * b[2 * k];
            out[2 * k + 1] = invMix * a[2 * k + 1] + mix * b[2 * k + 1];
        }
    }

    // -----------------------------------------------------------------------------
    // 2. Spectral Gain / Filtering
    // -----------------------------------------------------------------------------

    /**
     * @brief Apply a per-bin real-valued gain or EQ curve to packed RFFT data.
     *
     * @param[in,out] fft_data  Pointer to FFT data (interleaved Re/Im, length = `fft_size`).
     * @param[in]     gain      Array of gain coefficients for each bin (length = `fft_size/2 + 1`).
     * @param[in]     fft_size  FFT size.
     *
     * @details
     * Equivalent to frequency-domain filtering:
     * \f[
     * X'[k] = g[k] \cdot X[k]
     * \f]
     * where `gain[k]` scales both the real and imaginary components of each bin,
     * preserving the phase.
     *
     * | Bin | Indices | Description | Gain Index |
     * |:----|:---------|:-------------|:------------|
     * | DC | 0 | Real-only | gain[0] |
     * | Nyquist | 1 | Real-only | gain[N/2] |
     * | Complex bins | 2*k, 2*k+1 | Real/Imag pairs | gain[k] |
     *
     * @note
     * The `gain` array must have length **`fft_size/2 + 1`**, corresponding to all
     * unique frequency bins from 0 → Nyquist.
     */
    inline void ApplyWindowToSpectrum(float *fft_data, const float *gain, size_t fft_size)
    {
        const size_t half = fft_size / 2;
        fft_data[0] *= gain[0];    // DC bin
        fft_data[1] *= gain[half]; // Nyquist bin

        for (size_t k = 1; k < half; ++k)
        {
            fft_data[2 * k] *= gain[k];
            fft_data[2 * k + 1] *= gain[k];
        }
    }

    // -----------------------------------------------------------------------------
    // 3. Spectral Phase and Coherence
    // -----------------------------------------------------------------------------

    /**
     * @brief Align the phase of spectrum B to match spectrum A.
     *
     * @param[in]  magA     Magnitude array of spectrum A (length = N/2 + 1).
     * @param[in]  phaseA   Phase array of spectrum A (length = N/2 + 1).
     * @param[in,out] magB  Magnitude array of spectrum B (length = N/2 + 1).
     * @param[in,out] phaseB Phase array of spectrum B (length = N/2 + 1).
     * @param[in]  n_bins   Number of unique FFT bins (N/2 + 1).
     * @param[in]  blend    Phase blending factor [0–1]. (1 = full alignment)
     *
     * @details
     * Useful for phase-synchronous resynthesis or vocoding, ensuring coherence
     * between two spectral sources.
     */
    inline void PhaseAlign(const float *magA, const float *phaseA,
                           float *magB, float *phaseB,
                           size_t n_bins, float blend = 1.0f)
    {
        for (size_t k = 0; k < n_bins; ++k)
        {
            float diff = phaseA[k] - phaseB[k];
            while (diff > M_PI)
                diff -= 2.0f * static_cast<float>(M_PI);
            while (diff < -M_PI)
                diff += 2.0f * static_cast<float>(M_PI);
            phaseB[k] += diff * blend;
        }
    }

    /**
     * @brief Compute normalized spectral coherence between two RFFT spectra.
     *
     * @param[in] a        First packed RFFT buffer (length = `fft_size`).
     * @param[in] b        Second packed RFFT buffer (length = `fft_size`).
     * @param[in] fft_size FFT size.
     * @return Coherence value in range [0, 1].
     *
     * @details
     * \f[
     * C = \frac{|\sum_k A[k] B^*[k]|}{\sqrt{\sum_k |A[k]|^2 \sum_k |B[k]|^2}}
     * \f]
     *
     * Indicates the degree of similarity between two spectra:
     * - 1.0 = perfectly phase-aligned
     * - 0.0 = completely decorrelated
     */
    inline float SpectralCoherence(const float *a, const float *b, size_t fft_size)
    {
        const size_t half = fft_size / 2;
        float numRe = 0.0f, numIm = 0.0f;
        float sumA = 0.0f, sumB = 0.0f;

        for (size_t k = 1; k < half; ++k)
        {
            const float ar = a[2 * k], ai = a[2 * k + 1];
            const float br = b[2 * k], bi = b[2 * k + 1];

            numRe += ar * br + ai * bi;
            numIm += ai * br - ar * bi;
            sumA += ar * ar + ai * ai;
            sumB += br * br + bi * bi;
        }

        float denom = sqrtf(sumA * sumB + 1e-9f);
        return denom > 0.0f ? sqrtf(numRe * numRe + numIm * numIm) / denom : 0.0f;
    }

    // -----------------------------------------------------------------------------
    // 4. Spectral Manipulation and Analysis
    // -----------------------------------------------------------------------------

    /**
     * @brief Combine magnitudes from A with phases from B into a new packed spectrum.
     *
     * @param[in]  magA     Magnitude array (length = N/2 + 1).
     * @param[in]  phaseB   Phase array (length = N/2 + 1).
     * @param[out] out_fft  Output FFT buffer (length = N).
     * @param[in]  fft_size FFT size.
     *
     * @details
     * Constructs a new spectrum:
     * \f[
     * X[k] = |A[k]| e^{j \phi_B[k]}
     * \f]
     * Used in spectral morphing, vocoders, or resynthesis hybrids.
     */
    inline void SpectralMixWeightedPhase(const float *magA, const float *phaseB,
                                         float *out_fft, size_t fft_size)
    {
        const size_t half = fft_size / 2;
        out_fft[0] = magA[0];
        out_fft[1] = magA[half];

        for (size_t k = 1; k < half; ++k)
        {
            float m = magA[k];
            float p = phaseB[k];
            out_fft[2 * k] = m * cosf(p);
            out_fft[2 * k + 1] = m * sinf(p);
        }
    }

    /**
     * @brief Freeze bins where flux or energy is below threshold.
     *
     * @param[in,out] curr_fft   Current spectrum (length = N).
     * @param[in]     prev_fft   Previous spectrum (length = N).
     * @param[in]     flux_mag   Per-bin flux or energy measure (length = N/2 + 1).
     * @param[in]     fft_size   FFT size.
     * @param[in]     threshold  Energy threshold below which bins are frozen.
     *
     * @details
     * Commonly used in *spectral freeze* or *texture hold* effects.
     * Retains the last active frequency bins when motion falls below a threshold.
     */
    inline void SpectralFreeze(float *curr_fft,
                               const float *prev_fft,
                               const float *flux_mag,
                               size_t fft_size,
                               float threshold)
    {
        const size_t half = fft_size / 2;
        for (size_t k = 1; k < half; ++k)
        {
            if (flux_mag[k] < threshold)
            {
                curr_fft[2 * k] = prev_fft[2 * k];
                curr_fft[2 * k + 1] = prev_fft[2 * k + 1];
            }
        }
    }

    // -----------------------------------------------------------------------------
    // 5. Normalization and Utility
    // -----------------------------------------------------------------------------

    /**
     * @brief Uniformly scale all bins by a scalar gain.
     *
     * @param[in,out] fft_data  FFT buffer (length = `fft_size`).
     * @param[in]     gain      Scalar multiplier.
     * @param[in]     fft_size  FFT size.
     */
    inline void Scale(float *fft_data, float gain, size_t fft_size)
    {
        for (size_t i = 0; i < fft_size; ++i)
            fft_data[i] *= gain;
    }

    /**
     * @brief Compute total spectral energy of packed FFT data.
     *
     * @param[in] fft_data FFT buffer (length = `fft_size`).
     * @param[in] fft_size FFT size.
     * @return Total energy (sum of squared magnitudes).
     */
    inline float Energy(const float *fft_data, size_t fft_size)
    {
        const size_t half = fft_size / 2;
        float sum = fft_data[0] * fft_data[0] + fft_data[1] * fft_data[1];
        for (size_t k = 1; k < half; ++k)
        {
            const float re = fft_data[2 * k];
            const float im = fft_data[2 * k + 1];
            sum += re * re + im * im;
        }
        return sum;
    }

    /**
     * @brief Normalize FFT data so that total energy equals 1.0.
     *
     * @param[in,out] fft_data FFT buffer (length = `fft_size`).
     * @param[in]     fft_size FFT size.
     *
     * @details
     * Scales all bins by:
     * \f[
     * g = \frac{1}{\sqrt{E}}
     * \f]
     * where \(E\) is the total spectral energy.
     */
    inline void NormalizeEnergy(float *fft_data, size_t fft_size)
    {
        float e = Energy(fft_data, fft_size);
        if (e < 1e-9f)
            return;
        const float scale = 1.0f / sqrtf(e);
        Scale(fft_data, scale, fft_size);
    }

    /**
     * @brief Compute wrapped phase differences between consecutive phase arrays.
     *
     * @param[in]  curr_phase  Current frame phases (length = N/2 + 1).
     * @param[in]  prev_phase  Previous frame phases (length = N/2 + 1).
     * @param[out] out_diff    Output wrapped phase differences (length = N/2 + 1).
     * @param[in]  n_bins      Number of bins (`fft_size / 2 + 1`).
     *
     * @details
     * Computes Δϕ[k] = wrap(ϕₙ[k] − ϕₙ₋₁[k]) into the range [−π, π].
     * Used in phase vocoder analysis to estimate frequency deviations.
     */
    inline void PhaseDifference(const float *curr_phase,
                                const float *prev_phase,
                                float *out_diff,
                                size_t n_bins)
    {
        const float two_pi = 2.0f * static_cast<float>(M_PI);
        for (size_t k = 0; k < n_bins; ++k)
        {
            float d = curr_phase[k] - prev_phase[k];
            while (d > M_PI)
                d -= two_pi;
            while (d < -M_PI)
                d += two_pi;
            out_diff[k] = d;
        }
    }

} // namespace dsp::spectral::ops

/* EOF */
