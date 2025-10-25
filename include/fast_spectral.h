/**
 * @file fast_spectral.h
 * @brief Conversion utilities between complex FFT data and magnitude/phase representations.
 *
 * @details
 * The **Fast Spectral** conversion module provides efficient functions to convert between
 * packed real FFT (RFFT) buffers and their polar forms (magnitude and phase).
 *
 * These utilities serve as the foundation for spectral-domain processing, analysis,
 * and resynthesis. They are typically used with the FastDSP STFT framework and
 * CMSIS-DSP's real FFT implementations.
 *
 * Typical use cases include:
 * - Feature extraction (spectral centroid, flux, etc.)
 * - Spectral filtering, morphing, or resynthesis
 * - Visualization and metering
 *
 * @note
 * Functions are compatible with CMSIS-DSP `arm_rfft_fast_f32()` packed output format.
 *
 * @ingroup FastDSPSpectral
 * @defgroup FastDSPSpectralConversions Spectral Conversion Utilities
 * @brief Convert between packed FFT data and magnitude/phase representations.
 */

#pragma once
#include <cmath>
#include <cstddef>
#include "arm_math.h"

namespace dsp::spectral
{

    // -----------------------------------------------------------------------------
    // 1. Magnitude / Phase Conversion Utilities
    // -----------------------------------------------------------------------------

    /**
     * @brief Convert packed FFT output (complex bins) to magnitude and phase arrays.
     * @ingroup FastSpectralConversions
     *
     * @param[in]  fft_data Pointer to FFT output buffer (packed complex, length = `fft_size`).
     * @param[out] mags     Output magnitude array (length = `fft_size / 2 + 1`).
     * @param[out] phases   Output phase array (length = `fft_size / 2 + 1`).
     * @param[in]  fft_size Size of the full FFT frame (number of real input samples).
     *
     * @details
     * Converts CMSIS-DSP RFFT output (interleaved Re/Im pairs) into magnitude and phase arrays.
     *
     * - **DC (bin 0)** and **Nyquist (bin N/2)** are real-only and have zero phase.
     * - Phases are expressed in radians within the range [-π, +π].
     * - Magnitudes use CMSIS-DSP `arm_sqrt_f32()` for fast scalar computation.
     *
     * ### FFT Buffer Layout
     * ```
     * fft_data[0] = DC (real)
     * fft_data[1] = Nyquist (real)
     * fft_data[2*k]   = Re(bin k)
     * fft_data[2*k+1] = Im(bin k)
     * ```
     *
     * @see FromMagPhase()
     *
     * @par Example
     * @code
     * float mags[N_BINS], phases[N_BINS];
     * dsp::spectral::ToMagPhase(fft_out, mags, phases, FFT_SIZE);
     * @endcode
     */
    inline void ToMagPhase(const float *fft_data, float *mags, float *phases, size_t fft_size)
    {
        const size_t half = fft_size / 2;

        // DC bin (real-only)
        mags[0] = fft_data[0];
        phases[0] = 0.0f;

        // Complex bins (1 .. N/2 - 1)
        for (size_t k = 1; k < half; ++k)
        {
            const float re = fft_data[2 * k];
            const float im = fft_data[2 * k + 1];
            const float mag_sq = re * re + im * im;

            arm_sqrt_f32(mag_sq, &mags[k]);
            arm_atan2_f32(im, re, &phases[k]);
        }

        // Nyquist bin (real-only)
        mags[half] = fft_data[1];
        phases[half] = 0.0f;
    }

    /**
     * @brief Reconstruct packed complex FFT data from magnitude and phase arrays.
     * @ingroup FastSpectralConversions
     *
     * @param[in]  mags     Magnitude array (length = `fft_size / 2 + 1`).
     * @param[in]  phases   Phase array (length = `fft_size / 2 + 1`).
     * @param[out] fft_data Output FFT buffer (packed complex, length = `fft_size`).
     * @param[in]  fft_size FFT size (number of real input samples).
     *
     * @details
     * Recreates the interleaved real/imaginary FFT buffer expected by
     * `arm_rfft_fast_f32()` or `Fast_ISTFT` from polar spectral data.
     *
     * - **DC and Nyquist bins** are real-only.
     * - Other bins are reconstructed using cosine/sine of phase values.
     * - Ready for inverse FFT or further processing.
     *
     * @see ToMagPhase()
     *
     * @par Example
     * @code
     * float mags[N_BINS], phases[N_BINS], fft_buf[FFT_SIZE];
     * dsp::spectral::FromMagPhase(mags, phases, fft_buf, FFT_SIZE);
     * @endcode
     */
    inline void FromMagPhase(const float *mags, const float *phases, float *fft_data, size_t fft_size)
    {
        const size_t half = fft_size / 2;

        // DC and Nyquist bins (real-only)
        fft_data[0] = mags[0];
        fft_data[1] = mags[half];

        // Complex bins
        for (size_t k = 1; k < half; ++k)
        {
            const float mag = mags[k];
            const float phase = phases[k];
            fft_data[2 * k] = mag * arm_cos_f32(phase);
            fft_data[2 * k + 1] = mag * arm_sin_f32(phase);
        }
    }

    // -----------------------------------------------------------------------------
    // 2. Frequency Bin Calculation
    // -----------------------------------------------------------------------------

    /**
     * @brief Compute frequency bin center frequencies for a given FFT size and sample rate.
     *
     * @param[out] freqs       Output array of bin frequencies (length = `fft_size / 2 + 1`).
     * @param[in]  fft_size    FFT length in samples.
     * @param[in]  sample_rate Sampling rate in Hz.
     *
     * @details
     * Computes the center frequency for each FFT bin:
     * \f[
     * f_k = \frac{k \cdot f_s}{N}
     * \f]
     * where `f_s` is the sampling rate and `N` is the FFT size.
     *
     * ### Usage Notes
     * - Only non-negative frequencies (0–Nyquist) are produced.
     * - The resulting `freqs` array can be precomputed and reused for all
     *   subsequent spectral feature analyses.
     * - Commonly used with feature functions like `SpectralCentroid()` and `SpectralRolloff()`.
     *
     * ### Example
     * ```cpp
     * constexpr size_t FFT_SIZE = 1024;
     * float freqs[FFT_SIZE / 2 + 1];
     * dsp::spectral::ComputeFrequencyBins(freqs, FFT_SIZE, 48000.0f);
     * ```
     *
     * @ingroup FastSpectralFeatures
     */
    inline void ComputeFrequencyBins(float *freqs, size_t fft_size, float sample_rate)
    {
        const size_t n_bins = fft_size / 2 + 1;
        const float bin_hz = sample_rate / static_cast<float>(fft_size);
        for (size_t k = 0; k < n_bins; ++k)
            freqs[k] = k * bin_hz;
    }

} // namespace dsp::spectral

/* EOF */
