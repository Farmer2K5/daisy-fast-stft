/**
 * @file fast_rfft.h
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
