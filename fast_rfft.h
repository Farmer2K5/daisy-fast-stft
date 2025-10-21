/**
 * @file Fast_RFFT.hpp
 * @brief Lightweight, copyable wrapper for CMSIS-DSP real FFT (RFFT) transforms.
 *
 * @details
 * This header defines the `dsp::Fast_RFFT` class template — a minimal, type-safe,
 * and copyable C++ wrapper around the CMSIS-DSP `arm_rfft_fast_f32` API.
 * It supports forward and inverse real-valued FFT operations using the optimized
 * ARM Fast RFFT routines, providing both magnitude/phase conversions and
 * window utility helpers.
 *
 * **Key Features:**
 * - Uses CMSIS-DSP `arm_rfft_fast_instance_f32` for efficient FFT computation.
 * - Compile-time FFT size selection (supports 32–4096 samples).
 * - Forward and inverse transforms for real-valued signals.
 * - Built-in window generation and normalization utilities.
 * - Optional conversion between complex spectra and magnitude/phase.
 * - Fully copyable and instance-safe for embedded applications.
 *
 * **Typical Use:**
 * @code
 * dsp::Fast_RFFT<1024> fft;
 * float time_buf[1024];
 * float freq_buf[1024];
 *
 * fft.Forward(time_buf, freq_buf);
 * // ... process freq_buf ...
 * fft.Inverse(freq_buf, time_buf);
 * @endcode
 *
 * @note Designed for embedded DSP and real-time spectral processing
 *       on ARM Cortex-M microcontrollers using CMSIS-DSP.
 */

#pragma once
#include <cmath>
#include <cstdint>
#include "arm_math.h"

namespace dsp
{
    /**
     * @brief Wrapper around CMSIS-DSP fast real FFT instance.
     * @tparam kFftSize The FFT size in samples (must be one of 32, 64, 128, 256, 512, 1024, 2048, 4096).
     *
     * Provides instance-based initialization, forward and inverse transforms,
     * and helper functions for window generation and magnitude/phase conversion.
     */
    template <size_t kFftSize>
    class Fast_RFFT
    {
    public:
        /** @brief Construct and initialize the RFFT instance. */
        Fast_RFFT() { Init(); }

        /** @brief Copy constructor (safe, shallow copy of CMSIS instance). */
        Fast_RFFT(const Fast_RFFT &) = default;

        /** @brief Assignment operator (safe, shallow copy of CMSIS instance). */
        Fast_RFFT &operator=(const Fast_RFFT &) = default;

        /** @brief FFT size constant (samples). */
        static constexpr size_t FFT_SIZE = kFftSize;

        /** @return FFT size in samples. */
        static constexpr size_t size() { return FFT_SIZE; }

        /** @return Number of positive-frequency bins (`FFT_SIZE / 2 + 1`). */
        static constexpr size_t NumBins() { return FFT_SIZE / 2 + 1; }

        // --------------------------------------------------------------------
        // Core Transform Operations
        // --------------------------------------------------------------------

        /**
         * @brief Perform forward real-to-complex FFT.
         *
         * @param input  Pointer to real-valued time-domain samples (size `FFT_SIZE`).
         * @param output Pointer to FFT output buffer (packed complex, size `FFT_SIZE`).
         *
         * @details
         * The output format is CMSIS-DSP interleaved complex packing:
         * `[Re(0), Re(N/2), Re(1), Im(1), Re(2), Im(2), ...]`.
         */
        inline void Forward(const float *input, float *output) const noexcept
        {
            arm_rfft_fast_f32(&inst_, const_cast<float *>(input), output, 0);
        }

        /**
         * @brief Perform inverse complex-to-real FFT.
         *
         * @param input  Pointer to packed complex frequency-domain buffer (size `FFT_SIZE`).
         * @param output Pointer to real-valued time-domain buffer (size `FFT_SIZE`).
         */
        inline void Inverse(const float *input, float *output) const noexcept
        {
            arm_rfft_fast_f32(&inst_, const_cast<float *>(input), output, 1);
        }

        /**
         * @brief Zero out a buffer of length `FFT_SIZE`.
         * @param buf Pointer to buffer to clear.
         */
        inline void Zero(float *buf) const noexcept
        {
            for (size_t i = 0; i < FFT_SIZE; ++i)
                buf[i] = 0.0f;
        }

        // --------------------------------------------------------------------
        // Window Utilities
        // --------------------------------------------------------------------

        /** @brief Supported window function types. */
        enum class WindowType
        {
            Hann,     ///< Cosine-squared Hann window.
            Hamming,  ///< Hamming window (slightly higher sidelobes).
            Blackman, ///< Blackman window (very low sidelobes).
            Gaussian  ///< Gaussian window (requires `alpha` parameter).
        };

        /**
         * @brief Generate a window function of the specified type.
         * @param type   Window function type.
         * @param window Output buffer for generated window (size `FFT_SIZE`).
         * @param alpha  Gaussian width parameter (used only for `WindowType::Gaussian`).
         *
         * @details
         * For Gaussian windows, `alpha` controls the standard deviation.
         * Typical value: 0.4 (default).
         */
        static void MakeWindow(WindowType type, float *window, float alpha = 0.4f)
        {
            const float N = static_cast<float>(FFT_SIZE - 1);

            switch (type)
            {
            case WindowType::Hann:
                for (size_t i = 0; i < FFT_SIZE; ++i)
                    window[i] = 0.5f * (1.0f - arm_cos_f32(2.0f * PI * i / N));
                break;

            case WindowType::Hamming:
                for (size_t i = 0; i < FFT_SIZE; ++i)
                    window[i] = 0.54f - 0.46f * arm_cos_f32(2.0f * PI * i / N);
                break;

            case WindowType::Blackman:
                for (size_t i = 0; i < FFT_SIZE; ++i)
                    window[i] = 0.42f - 0.5f * arm_cos_f32(2.0f * PI * i / N) +
                                0.08f * arm_cos_f32(4.0f * PI * i / N);
                break;

            case WindowType::Gaussian:
            {
                const float sigma = alpha * (FFT_SIZE - 1) / 2.0f;
                const float denom = 2.0f * sigma * sigma;
                const float mid = (FFT_SIZE - 1) * 0.5f;
                for (size_t i = 0; i < FFT_SIZE; ++i)
                {
                    const float n = static_cast<float>(i) - mid;
                    window[i] = expf(-0.5f * (n * n) / denom);
                }
            }
            break;
            }
        }

        /**
         * @brief Apply a window in-place to a signal buffer.
         * @param window Pointer to the window coefficients (size `FFT_SIZE`).
         * @param data   Pointer to signal data (modified in-place).
         */
        static void ApplyWindow(const float *window, float *data)
        {
            arm_mult_f32(data, window, data, FFT_SIZE);
        }

        /** @brief Normalization modes for window scaling. */
        enum class NormType
        {
            Sum, ///< Normalize by total sum of coefficients.
            RMS  ///< Normalize by root-mean-square energy.
        };

        /**
         * @brief Normalize a window by sum or RMS.
         * @param window Pointer to window coefficients (modified in-place).
         * @param type   Normalization method (`Sum` or `RMS`).
         */
        static void NormalizeWindow(float *window, NormType type)
        {
            float sum = 0.0f, rms = 0.0f;
            for (uint16_t i = 0; i < FFT_SIZE; ++i)
            {
                sum += window[i];
                rms += window[i] * window[i];
            }

            if (type == NormType::Sum)
            {
                const float gain = 1.0f / sum;
                arm_scale_f32(window, gain, window, FFT_SIZE);
            }
            else if (type == NormType::RMS)
            {
                rms = sqrtf(rms / FFT_SIZE);
                const float gain = 1.0f / rms;
                arm_scale_f32(window, gain, window, FFT_SIZE);
            }
        }

        // --------------------------------------------------------------------
        // Magnitude / Phase Conversion Utilities
        // --------------------------------------------------------------------

        /**
         * @brief Convert FFT output (complex bins) to magnitude and phase.
         *
         * @param fft_data Pointer to FFT output buffer (packed complex, size `FFT_SIZE`).
         * @param mags     Output magnitudes array (size `FFT_SIZE / 2 + 1`).
         * @param phases   Output phases array (size `FFT_SIZE / 2 + 1`).
         *
         * @details
         * - DC (bin 0) and Nyquist (bin N/2) bins are real-only.
         * - Phases are returned in radians.
         */
        static void ToMagPhase(const float *fft_data, float *mags, float *phases)
        {
            size_t half = FFT_SIZE / 2;

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
         * @param mags     Input magnitudes array (size `FFT_SIZE / 2 + 1`).
         * @param phases   Input phases array (size `FFT_SIZE / 2 + 1`).
         * @param fft_data Output packed complex array (size `FFT_SIZE`).
         */
        static void FromMagPhase(const float *mags, const float *phases, float *fft_data)
        {
            size_t half = FFT_SIZE / 2;

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

    private:
        /** @brief CMSIS-DSP FFT instance handle. */
        arm_rfft_fast_instance_f32 inst_;

        /**
         * @brief Initialize the CMSIS-DSP FFT instance for the selected size.
         *
         * @details
         * Uses compile-time dispatch to call the correct `arm_rfft_fast_init_<SIZE>_f32()`
         * specialization. A static assertion ensures unsupported sizes fail at compile time.
         */
        inline void Init()
        {
            if constexpr (FFT_SIZE == 32)
                arm_rfft_fast_init_32_f32(&inst_);
            else if constexpr (FFT_SIZE == 64)
                arm_rfft_fast_init_64_f32(&inst_);
            else if constexpr (FFT_SIZE == 128)
                arm_rfft_fast_init_128_f32(&inst_);
            else if constexpr (FFT_SIZE == 256)
                arm_rfft_fast_init_256_f32(&inst_);
            else if constexpr (FFT_SIZE == 512)
                arm_rfft_fast_init_512_f32(&inst_);
            else if constexpr (FFT_SIZE == 1024)
                arm_rfft_fast_init_1024_f32(&inst_);
            else if constexpr (FFT_SIZE == 2048)
                arm_rfft_fast_init_2048_f32(&inst_);
            else if constexpr (FFT_SIZE == 4096)
                arm_rfft_fast_init_4096_f32(&inst_);
            else
                static_assert(
                    FFT_SIZE == 32 || FFT_SIZE == 64 || FFT_SIZE == 128 ||
                        FFT_SIZE == 256 || FFT_SIZE == 512 || FFT_SIZE == 1024 ||
                        FFT_SIZE == 2048 || FFT_SIZE == 4096,
                    "Unsupported FFT size for arm_rfft_fast_init_*_f32()");
        }
    };

} // namespace dsp

/* EOF */
