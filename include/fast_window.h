/**
 * @file fast_window.h
 * @brief Window generation, normalization, and application utilities for DSP.
 *
 * @details
 * Provides reusable windowing functions for spectral processing.
 * Includes Hann, Hamming, Blackman, and Gaussian windows, with
 * optional normalization and in-place application utilities.
 */

#pragma once
#include <cmath>
#include <cstddef>
#include "arm_math.h"

namespace dsp
{
    /** @brief Supported window function types. */
    enum class WindowType
    {
        Hann,
        Hamming,
        Blackman,
        Gaussian
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
     * @param alpha  Gaussian width parameter (only used for Gaussian windows).
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
            break;
        }
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
