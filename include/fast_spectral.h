/**
 * @file fast_spectral.h
 * @brief Spectral-domain utilities for magnitude, phase, and feature analysis.
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
     * @brief Convert FFT output (complex bins) to magnitude and phase.
     *
     * @param fft_data Pointer to FFT output buffer (packed complex, size `fft_size`).
     * @param mags     Output magnitudes array (size `fft_size / 2 + 1`).
     * @param phases   Output phases array (size `fft_size / 2 + 1`).
     *
     * @details
     * - DC (bin 0) and Nyquist (bin N/2) bins are real-only.
     * - Phases are returned in radians.
     */
    inline void ToMagPhase(const float *fft_data, float *mags, float *phases, size_t fft_size)
    {
        size_t half = fft_size / 2;

        mags[0] = fft_data[0];
        phases[0] = 0.0f;

        for (size_t k = 1; k < half; ++k)
        {
            float re = fft_data[2 * k];
            float im = fft_data[2 * k + 1];
            float mag_sq = re * re + im * im;
            arm_sqrt_f32(mag_sq, &mags[k]);
            arm_atan2_f32(im, re, &phases[k]);
        }

        mags[half] = fft_data[1];
        phases[half] = 0.0f;
    }

    /**
     * @brief Convert magnitude and phase arrays back into complex FFT format.
     *
     * @param mags     Input magnitudes array (size `fft_size / 2 + 1`).
     * @param phases   Input phases array (size `fft_size / 2 + 1`).
     * @param fft_data Output packed complex array (size `fft_size`).
     */
    inline void FromMagPhase(const float *mags, const float *phases, float *fft_data, size_t fft_size)
    {
        size_t half = fft_size / 2;

        fft_data[0] = mags[0];
        fft_data[1] = mags[half];

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
