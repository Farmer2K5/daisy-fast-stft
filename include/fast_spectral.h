/**
 * @file fast_spectral.h
 * @brief Spectral-domain utilities for magnitude, phase, and feature analysis.
 *
 * @details
 * This header provides inline functions for converting between complex FFT data
 * (real/imag interleaved format) and polar representations (magnitude and phase).
 *
 * These are the core building blocks for frequency-domain audio processing:
 * - Magnitude and phase extraction for analysis or feature computation
 * - Reconstruction of complex FFT frames for resynthesis
 *
 * @note Designed to complement `Fast_RFFT`, `Fast_STFT`, and `Fast_ISTFT`.
 * All functions use CMSIS-DSP intrinsics (`arm_sqrt_f32`, `arm_atan2_f32`, etc.)
 * for efficient execution on ARM Cortex-M processors.
 *
 * @ingroup FastDSP
 * @defgroup FastSpectral Spectral Utilities
 * @ingroup FastDSP
 * @brief Utilities for spectral-domain analysis, magnitude/phase manipulation,
 *        and reconstruction.
 */

#pragma once
#include <cmath>
#include <cstddef>
#include "arm_math.h"

namespace dsp
{

    // --------------------------------------------------------------------
    // Magnitude / Phase Conversion Utilities
    // --------------------------------------------------------------------

    /**
     * @brief Convert FFT output (complex bins) to magnitude and phase arrays.
     * @ingroup FastSpectral
     *
     * @param fft_data Pointer to FFT output buffer (packed complex, size = `fft_size`).
     * @param mags     Output magnitudes array (size = `fft_size / 2 + 1`).
     * @param phases   Output phases array (size = `fft_size / 2 + 1`).
     * @param fft_size Size of the full FFT frame (number of real samples).
     *
     * @details
     * Converts the packed real FFT output (as produced by `arm_rfft_fast_f32()`)
     * into magnitude and phase arrays for each bin.
     *
     * - **DC (bin 0)** and **Nyquist (bin N/2)** are real-only and have zero phase.
     * - Phases are returned in **radians** in the range [-π, +π].
     * - Magnitudes are computed using `sqrt(re² + im²)` with CMSIS-DSP `arm_sqrt_f32()`.
     * - Use these arrays for spectral-domain operations such as:
     *   - Spectral filtering
     *   - Spectral morphing or resynthesis
     *   - Feature analysis (centroid, flux, etc.)
     *
     * @note
     * The FFT buffer layout must match CMSIS-DSP's **real FFT (RFFT)** convention:
     * ```
     * X[0] = DC (real)
     * X[1] = Nyquist (real)
     * X[2], X[3] = Re(X[1]), Im(X[1])
     * X[4], X[5] = Re(X[2]), Im(X[2])
     * ...
     * ```
     *
     * @see FromMagPhase()
     *
     * @par Example
     * @code
     * float mags[N_BINS], phases[N_BINS];
     * dsp::ToMagPhase(fft_out, mags, phases, FFT_SIZE);
     * @endcode
     */
    inline void ToMagPhase(const float *fft_data, float *mags, float *phases, size_t fft_size)
    {
        size_t half = fft_size / 2;

        // DC bin (pure real)
        mags[0] = fft_data[0];
        phases[0] = 0.0f;

        // Process bins 1 .. N/2 - 1
        for (size_t k = 1; k < half; ++k)
        {
            float re = fft_data[2 * k];
            float im = fft_data[2 * k + 1];
            float mag_sq = re * re + im * im;

            // Compute magnitude and phase using CMSIS intrinsics
            arm_sqrt_f32(mag_sq, &mags[k]);
            arm_atan2_f32(im, re, &phases[k]);
        }

        // Nyquist bin (pure real)
        mags[half] = fft_data[1];
        phases[half] = 0.0f;
    }

    /**
     * @brief Reconstruct complex FFT data from magnitude and phase arrays.
     * @ingroup FastSpectral
     *
     * @param mags     Input magnitudes array (size = `fft_size / 2 + 1`).
     * @param phases   Input phases array (size = `fft_size / 2 + 1`).
     * @param fft_data Output complex FFT array (packed format, size = `fft_size`).
     * @param fft_size FFT size (number of real samples).
     *
     * @details
     * Converts polar spectral data (magnitude and phase) back into packed
     * complex FFT format, ready for inverse FFT (e.g., via `Fast_RFFT::Inverse()`).
     *
     * - Magnitude and phase are converted as:
     *   ```
     *   Re[k] = mag[k] * cos(phase[k])
     *   Im[k] = mag[k] * sin(phase[k])
     *   ```
     * - DC and Nyquist bins are real-only.
     * - Output format matches CMSIS-DSP's RFFT convention.
     *
     * @note
     * This function is the inverse of `ToMagPhase()` and is typically used
     * in conjunction with `Fast_STFT` or `Fast_ISTFT` for spectral-domain
     * effects and resynthesis.
     *
     * @par Example
     * @code
     * float mags[N_BINS], phases[N_BINS];
     * // modify spectral data...
     * dsp::FromMagPhase(mags, phases, fft_buf, FFT_SIZE);
     * fft.Inverse(fft_buf, time_signal);
     * @endcode
     */
    inline void FromMagPhase(const float *mags, const float *phases, float *fft_data, size_t fft_size)
    {
        size_t half = fft_size / 2;

        // DC and Nyquist bins (real-only)
        fft_data[0] = mags[0];
        fft_data[1] = mags[half];

        // Fill real/imag pairs for 1..N/2 - 1
        for (size_t k = 1; k < half; ++k)
        {
            float mag = mags[k];
            float phase = phases[k];
            fft_data[2 * k] = mag * arm_cos_f32(phase);
            fft_data[2 * k + 1] = mag * arm_sin_f32(phase);
        }
    }

} // namespace dsp

/* EOF */
