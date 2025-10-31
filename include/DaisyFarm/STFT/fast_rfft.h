/**
 * @file fast_rfft.h
 * @brief Lightweight C++ wrapper for CMSIS-DSP real FFT (RFFT) transforms.
 *
 * @details
 * The `daisyfarm::Fast_RFFT` class provides a thin, type-safe, and copyable wrapper
 * around the CMSIS-DSP `arm_rfft_fast_f32` API. It simplifies FFT operations
 * for embedded DSP development and integrates cleanly with the other components
 * of the FastDSP framework.
 *
 * Features:
 * - Compile-time FFT size selection (32–4096 samples)
 * - Fast real-valued forward/inverse FFTs using CMSIS-DSP
 * - Compatible with magnitude/phase conversions and windowing utilities
 * - Fully copyable and safe for use in real-time audio callbacks
 *
 * **Typical Use:**
 * @code
 * daisyfarm::Fast_RFFT<1024> fft;
 * float time_buf[1024];
 * float freq_buf[1024];
 *
 * fft.Forward(time_buf, freq_buf); // Real → Frequency domain
 * // ... spectral processing ...
 * fft.Inverse(freq_buf, time_buf); // Frequency → Real domain
 * @endcode
 *
 * @ingroup FastDSPCore
 * @defgroup FastDSPCoreFFT Fast Real FFT Utilities
 * @brief Wrapper and helper functions for CMSIS-DSP real FFTs.
 *
 * @note
 * Designed for real-time spectral processing on ARM Cortex-M7/M4 microcontrollers
 * using the optimized CMSIS-DSP library. It can be combined directly with
 * `Fast_STFT` and `Fast_ISTFT` for full short-time spectral workflows.
 */

#pragma once
#include <cmath>
#include <cstdint>
#include "arm_math.h"

namespace daisyfarm
{

    // -------------------------------------------------------------------------
    // Fast_RFFT Class
    // -------------------------------------------------------------------------

    /**
     * @class Fast_RFFT
     * @brief Simple C++ wrapper for CMSIS-DSP real FFT transforms.
     *
     * @tparam kFftSize FFT size in samples (must be one of:
     *         32, 64, 128, 256, 512, 1024, 2048, 4096).
     *
     * @details
     * Provides fast real-to-complex and complex-to-real FFT transforms using
     * the CMSIS-DSP optimized FFT routines. Supports seamless integration
     * with windowing and spectral conversion functions.
     *
     * Each instance contains its own `arm_rfft_fast_instance_f32`, allowing
     * multiple FFT objects with different sizes to coexist safely.
     *
     * The FFT operates in-place on single-precision floats (`float32_t`).
     */
    template <size_t kFftSize>
    class Fast_RFFT
    {
    public:
        /** @brief FFT size constant (number of time-domain samples). */
        static constexpr size_t FFT_SIZE = kFftSize;

        /**
         * @brief Construct and initialize the FFT instance.
         *
         * @details
         * Automatically initializes the CMSIS-DSP FFT instance for the given
         * compile-time FFT size. All supported sizes (32–4096) are handled via
         * compile-time specialization.
         */
        Fast_RFFT() { Init(); }

        /** @brief Copy constructor (safe shallow copy of CMSIS instance). */
        Fast_RFFT(const Fast_RFFT &) = default;

        /** @brief Assignment operator (safe shallow copy of CMSIS instance). */
        Fast_RFFT &operator=(const Fast_RFFT &) = default;

        // ---------------------------------------------------------------------
        // Constants
        // ---------------------------------------------------------------------

        /** @return FFT size (number of input/output samples). */
        static constexpr size_t size() { return FFT_SIZE; }

        /** @return Number of unique frequency bins (FFT_SIZE / 2 + 1). */
        static constexpr size_t NumBins() { return FFT_SIZE / 2 + 1; }

        // ---------------------------------------------------------------------
        // Transform Operations
        // ---------------------------------------------------------------------

        /**
         * @brief Perform forward real-to-complex FFT.
         * @ingroup FastFFT
         *
         * @param input  Pointer to real-valued time-domain samples (`FFT_SIZE`).
         * @param output Pointer to frequency-domain buffer (`FFT_SIZE` floats).
         *
         * @details
         * Computes a forward real FFT using `arm_rfft_fast_f32()`. The output
         * buffer is in **CMSIS packed complex format**:
         * ```
         * X[0]        = Re(DC)
         * X[1]        = Re(Nyquist)
         * X[2*k]      = Re(bin k)
         * X[2*k + 1]  = Im(bin k)
         * ```
         *
         * Use with `ToMagPhase()` (from `fast_spectral.h`) for magnitude/phase analysis.
         *
         * @note
         * The function does not modify the input buffer.
         *
         * @par Example
         * @code
         * fft.Forward(time_signal, fft_out);
         * daisyfarm::ToMagPhase(fft_out, mags, phases, FFT_SIZE);
         * @endcode
         */
        inline void Forward(const float *input, float *output) const noexcept
        {
            arm_rfft_fast_f32(&inst_, const_cast<float *>(input), output, 0);
        }

        /**
         * @brief Perform inverse complex-to-real FFT.
         * @ingroup FastFFT
         *
         * @param input  Pointer to packed complex frequency-domain buffer.
         * @param output Pointer to real-valued time-domain buffer (`FFT_SIZE`).
         *
         * @details
         * Reconstructs a real-valued signal from its complex spectral representation.
         * The input must follow the CMSIS-DSP RFFT format as described in `Forward()`.
         *
         * @note
         * The output is unnormalized; if your processing chain requires amplitude
         * matching, apply gain correction externally or use COLA normalization
         * in `Fast_STFT` or `Fast_ISTFT`.
         */
        inline void Inverse(const float *input, float *output) const noexcept
        {
            arm_rfft_fast_f32(&inst_, const_cast<float *>(input), output, 1);
        }

        /**
         * @brief Zero out a buffer of length `FFT_SIZE`.
         *
         * @param buf Pointer to buffer to clear.
         *
         * @details
         * Convenience function to quickly clear a time-domain or frequency-domain
         * buffer before reuse.
         *
         * @par Example
         * @code
         * fft.Zero(fft_out);
         * @endcode
         */
        inline void Zero(float *buf) const noexcept
        {
            for (size_t i = 0; i < FFT_SIZE; ++i)
                buf[i] = 0.0f;
        }

    private:
        /** @brief Internal CMSIS-DSP FFT instance. */
        arm_rfft_fast_instance_f32 inst_;

        /**
         * @brief Initialize the CMSIS-DSP FFT instance.
         *
         * @details
         * This uses compile-time dispatch to the correct
         * `arm_rfft_fast_init_<SIZE>_f32()` function.
         *
         * @warning Unsupported FFT sizes will trigger a compile-time error.
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

} // namespace daisyfarm

/* EOF */
