/**
 * @file fast_window.h
 * @brief Window generation, normalization, and application utilities for DSP.
 *
 * @details
 * Provides reusable windowing functions for spectral audio processing.
 * Includes Hann, Hamming, Blackman, Gaussian, and Tukey (tapered cosine) windows.
 * Uses CMSIS-DSP math functions for efficient computation.
 *
 * Designed for real-time STFT and spectral effects on ARM Cortex-M targets.
 */

#pragma once
#include <cstddef>
#include "arm_math.h"

namespace dsp
{
    /** @brief Supported window function types. */
    enum class WindowType
    {
        Hann,     ///< Smooth cosine-squared window (perfect COLA).
        Hamming,  ///< Slightly brighter variant of Hann.
        Blackman, ///< Very smooth, lower side lobes.
        Gaussian, ///< Symmetric Gaussian window, softest tone.
        Tukey     ///< Tapered cosine with adjustable sharpness (alpha).
    };

    /** @brief Normalization modes for window scaling. */
    enum class NormType
    {
        Sum,
        RMS
    };

    /**
     * @brief Generate a window function.
     *
     * @param type   Window function type.
     * @param window Output buffer (size = `size`).
     * @param size   Number of samples.
     * @param alpha  Shape parameter:
     *               - For Gaussian: width control (0.3–0.6 typical)
     *               - For Tukey: taper ratio (0–1, 0=rectangular, 1=Hann)
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
                window[i] = 0.42f - 0.5f * arm_cos_f32(2.0f * PI * i / N) +
                            0.08f * arm_cos_f32(4.0f * PI * i / N);
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
                        // Flat section
                        window[i] = 1.0f;
                    }
                }
            }
        }
        break;
        }
    }

    /** @brief Normalize a window (by sum or RMS). */
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

    /** @brief Apply a window in-place to a signal buffer. */
    inline void ApplyWindow(const float *window, float *data, size_t size)
    {
        arm_mult_f32(data, window, data, size);
    }

} // namespace dsp

/* EOF */
