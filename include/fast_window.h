/**
 * @file fast_window.h
 * @brief Window generation, normalization, and application utilities for DSP.
 *
 * @details
 * This module provides reusable windowing utilities for real-time spectral audio
 * processing. It supports common analytical and synthesis windows, including:
 * - Hann (cosine-squared)
 * - Hamming (slightly brighter Hann)
 * - Blackman (low side lobes, very smooth)
 * - Gaussian (soft, bell-shaped)
 * - Tukey (tapered cosine with variable flat region)
 *
 * All calculations use CMSIS-DSP math intrinsics (`arm_cos_f32`, `arm_mult_f32`,
 * and `arm_scale_f32`) for efficient operation on ARM Cortex-M processors.
 *
 * These utilities are designed for integration with:
 * - `Fast_STFT` (short-time Fourier transform)
 * - `Fast_ISTFT` (inverse short-time Fourier transform)
 * - `Fast_RFFT` (real FFT wrapper)
 *
 * @ingroup FastDSPCore
 * @defgroup FastDSPCoreWindow Windowing Functions
 * @brief Runtime window creation, normalization, and application.
 *
 * @note
 * Windows are used in spectral audio systems to reduce spectral leakage and ensure
 * proper constant overlap-add (COLA) reconstruction in STFT-based effects.
 */

#pragma once
#include <cstddef>
#include "arm_math.h"

namespace dsp
{

    // -------------------------------------------------------------------------
    // Enumerations
    // -------------------------------------------------------------------------

    /**
     * @enum WindowType
     * @brief Supported window function types.
     *
     * @details
     * Each window shape has different time-frequency trade-offs:
     * - **Hann**: Smooth, well-behaved COLA window; ideal general-purpose choice.
     * - **Hamming**: Slightly higher side lobes, brighter sound.
     * - **Blackman**: Very smooth, excellent side-lobe suppression.
     * - **Gaussian**: Soft, round window with adjustable width.
     * - **Tukey**: Tapered cosine; controllable between rectangular and Hann.
     */
    enum class WindowType
    {
        Hann,     ///< Cosine-squared window (perfect COLA behavior).
        Hamming,  ///< Slightly brighter variant of Hann.
        Blackman, ///< Smooth, low-sidelobe window for critical analysis.
        Gaussian, ///< Symmetric bell-shaped window for soft, continuous sound.
        Tukey     ///< Tapered cosine with flat center (alpha controls taper).
    };

    /**
     * @enum NormType
     * @brief Normalization modes for scaling window amplitude.
     *
     * @details
     * Windows can be normalized in different ways depending on usage:
     * - **Sum**: Scales the total sum of window samples to 1.0 (useful for FIR filters).
     * - **RMS**: Scales the RMS (energy) of the window to 1.0 (common for STFT use).
     */
    enum class NormType
    {
        Sum, ///< Normalize window by its total sum.
        RMS  ///< Normalize window by its RMS power.
    };

    // -------------------------------------------------------------------------
    // Window Generation
    // -------------------------------------------------------------------------

    /**
     * @brief Generate a window function.
     * @ingroup FastWindow
     *
     * @param type   Window function type (see ::WindowType).
     * @param window Output buffer (float array of length `size`).
     * @param size   Number of samples in the window.
     * @param alpha  Shape parameter:
     *               - For **Gaussian**, controls width (0.3–0.6 typical).
     *               - For **Tukey**, controls taper ratio (0–1; 0 = rectangular, 1 = Hann).
     *
     * @details
     * This function fills the `window` buffer with coefficients for the requested
     * window shape. Most window functions are symmetric about the center sample.
     *
     * @par Example
     * @code
     * constexpr size_t FFT_SIZE = 1024;
     * float window[FFT_SIZE];
     * dsp::MakeWindow(dsp::WindowType::Hann, window, FFT_SIZE);
     * @endcode
     *
     * @note
     * - Uses CMSIS-DSP math for efficient cosine computation.
     * - For Tukey windows, alpha defines the proportion of cosine taper.
     * - For Gaussian windows, smaller alpha → narrower bell.
     */
    inline void MakeWindow(WindowType type, float *window, size_t size, float alpha = 0.4f)
    {
        const float N = static_cast<float>(size - 1);

        switch (type)
        {
        case WindowType::Hann:
            for (size_t i = 0; i < size; ++i)
                window[i] = 0.5f * (1.0f - arm_cos_f32(2.0f * PI * i / N));
            break;

        case WindowType::Hamming:
            for (size_t i = 0; i < size; ++i)
                window[i] = 0.54f - 0.46f * arm_cos_f32(2.0f * PI * i / N);
            break;

        case WindowType::Blackman:
            for (size_t i = 0; i < size; ++i)
                window[i] = 0.42f - 0.5f * arm_cos_f32(2.0f * PI * i / N) + 0.08f * arm_cos_f32(4.0f * PI * i / N);
            break;

        case WindowType::Gaussian:
        {
            const float sigma = alpha * (size - 1) / 2.0f;
            const float denom = 2.0f * sigma * sigma;
            const float mid = (size - 1) * 0.5f;
            for (size_t i = 0; i < size; ++i)
            {
                const float n = static_cast<float>(i) - mid;
                window[i] = expf(-0.5f * (n * n) / denom);
            }
        }
        break;

        case WindowType::Tukey:
        {
            // Alpha defines proportion of cosine taper (0=rectangular, 1=Hann)
            if (alpha <= 0.0f)
            {
                for (size_t i = 0; i < size; ++i)
                    window[i] = 1.0f;
            }
            else if (alpha >= 1.0f)
            {
                for (size_t i = 0; i < size; ++i)
                    window[i] = 0.5f * (1.0f - arm_cos_f32(2.0f * PI * i / N));
            }
            else
            {
                const size_t taper = static_cast<size_t>(alpha * N / 2.0f);
                for (size_t i = 0; i < size; ++i)
                {
                    if (i < taper)
                    {
                        // Rising cosine
                        float x = static_cast<float>(i) / taper;
                        window[i] = 0.5f * (1.0f - arm_cos_f32(PI * x));
                    }
                    else if (i > N - taper)
                    {
                        // Falling cosine
                        float x = (static_cast<float>(i) - N + taper) / taper;
                        window[i] = 0.5f * (1.0f + arm_cos_f32(PI * x));
                    }
                    else
                    {
                        // Flat region
                        window[i] = 1.0f;
                    }
                }
            }
        }
        break;
        }
    }

    // -------------------------------------------------------------------------
    // Normalization
    // -------------------------------------------------------------------------

    /**
     * @brief Normalize a window function by sum or RMS.
     * @ingroup FastWindow
     *
     * @param window Pointer to window array (modified in place).
     * @param size   Number of samples in the window.
     * @param type   Normalization type (see ::NormType).
     *
     * @details
     * This function scales the window so that either:
     * - The **sum of samples** equals 1.0 (`NormType::Sum`), or
     * - The **root mean square** (energy) equals 1.0 (`NormType::RMS`).
     *
     * @par Example
     * @code
     * dsp::NormalizeWindow(window, 1024, dsp::NormType::RMS);
     * @endcode
     *
     * @note
     * RMS normalization is recommended for STFT windows to maintain
     * consistent amplitude during overlap-add reconstruction.
     */
    inline void NormalizeWindow(float *window, size_t size, NormType type)
    {
        float sum = 0.0f, rms = 0.0f;
        for (size_t i = 0; i < size; ++i)
        {
            sum += window[i];
            rms += window[i] * window[i];
        }

        if (type == NormType::Sum)
        {
            const float gain = 1.0f / sum;
            arm_scale_f32(window, gain, window, size);
        }
        else
        {
            rms = sqrtf(rms / size);
            const float gain = 1.0f / rms;
            arm_scale_f32(window, gain, window, size);
        }
    }

    // -------------------------------------------------------------------------
    // Application
    // -------------------------------------------------------------------------

    /**
     * @brief Apply a window to a signal buffer in place.
     * @ingroup FastWindow
     *
     * @param window Pointer to precomputed window coefficients.
     * @param data   Pointer to signal buffer (modified in place).
     * @param size   Number of samples to process.
     *
     * @details
     * Performs element-wise multiplication:
     * \f[
     * y[n] = x[n] \cdot w[n]
     * \f]
     * using the CMSIS-DSP function `arm_mult_f32()`.
     *
     * Commonly used before FFT analysis or after inverse FFT synthesis in
     * STFT frameworks to apply or reapply windowing functions.
     *
     * @par Example
     * @code
     * dsp::ApplyWindow(window, audio_frame, 1024);
     * @endcode
     */
    inline void ApplyWindow(const float *window, float *data, size_t size)
    {
        arm_mult_f32(data, window, data, size);
    }

} // namespace dsp

/* EOF */
