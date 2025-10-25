/**
 * @file fast_istft.h
 * @brief Inverse Short-Time Fourier Transform (ISTFT) framework.
 *
 * @details
 * This class reconstructs a time-domain audio signal from spectral (FFT)
 * frames produced by an STFT process.
 *
 * It performs:
 *  - Windowed inverse FFT (using Fast_RFFT)
 *  - Overlap-add accumulation
 *  - COLA (constant overlap-add) gain normalization
 *
 * Designed for use with frames produced by Fast_STFT or compatible
 * frequency-domain processors.
 *
 * @note This header is optional. You typically don't need it for real-time
 *       STFT effects since Fast_STFT already handles inverse synthesis.
 *       However, it is useful for offline spectral editing, modular DSP graphs,
 *       or machine-learning-based synthesis pipelines.
 */

#pragma once
#include "fast_rfft.h"
#include "fast_window.h"
#include "arm_math.h"
#include <cstring>

namespace dsp
{
    /**
     * @class Fast_ISTFT
     * @brief Performs inverse overlap-add reconstruction from spectral frames.
     *
     * @tparam kFftSize   FFT size (must match analysis FFT)
     * @tparam kHopSize   Hop size (must match analysis hop)
     */
    template <size_t kFftSize, size_t kHopSize>
    class Fast_ISTFT
    {
    public:
        /** @brief FFT size (samples). */
        static constexpr size_t FFT_SIZE = kFftSize;

        /** @brief Hop size (samples). */
        static constexpr size_t HOP_SIZE = kHopSize;

        /** @brief Number of complex bins in a real-valued FFT. */
        static constexpr size_t N_BINS = FFT_SIZE / 2 + 1;

        /**
         * @brief Construct a new Fast_ISTFT object.
         *
         * Initializes window, overlap buffer, and COLA gain.
         * Default window is Hann.
         */
        Fast_ISTFT()
            : read_pos_(0), cola_gain_(1.0f)
        {
            dsp::MakeWindow(dsp::WindowType::Hann, window_, FFT_SIZE);
            std::memset(overlap_buf_, 0, sizeof(overlap_buf_));
            ComputeCOLAGain_Linear();
        }

        /**
         * @brief Reset the internal overlap buffer and read position.
         *
         * Use this when starting a new reconstruction or clearing state.
         */
        void Reset()
        {
            std::memset(overlap_buf_, 0, sizeof(overlap_buf_));
            read_pos_ = 0;
        }

        /**
         * @brief Set a different synthesis window.
         *
         * @param type  Window function type (Hann, Hamming, etc.)
         * @param alpha Optional shape parameter for Tukey/Gaussian windows.
         */
        void SetWindow(WindowType type, float alpha = 0.4f)
        {
            dsp::MakeWindow(type, window_, FFT_SIZE, alpha);
            ComputeCOLAGain_Linear();
        }

        /**
         * @brief Process one inverse STFT frame.
         *
         * @param fft_data Complex FFT buffer (interleaved real/imag pairs)
         * @param output   Output buffer (HOP_SIZE samples)
         *
         * @details
         * The function performs:
         *  - Inverse FFT
         *  - Window application
         *  - Overlap-add accumulation
         *  - Output of the next hop-sized segment
         */
        void ProcessFrame(float *fft_data, float *output)
        {
            // 1. Inverse FFT
            fft_.Inverse(fft_data, time_buf_);

            // 2. Apply synthesis window
            dsp::ApplyWindow(window_, time_buf_, FFT_SIZE);

            // 3. Accumulate overlap-add region
            for (size_t i = 0; i < FFT_SIZE; ++i)
            {
                size_t idx = (read_pos_ + i) % FFT_SIZE;
                overlap_buf_[idx] += time_buf_[i] * cola_gain_;
            }

            // 4. Output hop-sized chunk
            for (size_t i = 0; i < HOP_SIZE; ++i)
            {
                output[i] = overlap_buf_[(read_pos_ + i) % FFT_SIZE];
                overlap_buf_[(read_pos_ + i) % FFT_SIZE] = 0.0f;
            }

            // 5. Advance pointer
            read_pos_ = (read_pos_ + HOP_SIZE) % FFT_SIZE;
        }

    private:
        /**
         * @brief Compute the constant overlap-add (COLA) normalization gain.
         *
         * Ensures perfect reconstruction amplitude given the current window
         * and hop size.
         */
        void ComputeCOLAGain_Linear()
        {
            const size_t overlap = FFT_SIZE / HOP_SIZE;
            float accum[HOP_SIZE] = {0.0f};

            for (size_t i = 0; i < overlap; ++i)
            {
                for (size_t j = 0; j < HOP_SIZE; ++j)
                {
                    size_t idx = j + i * HOP_SIZE;
                    if (idx < FFT_SIZE)
                        accum[j] += window_[idx] * window_[idx];
                }
            }

            float sum = 0.0f;
            for (size_t j = 0; j < HOP_SIZE; ++j)
                sum += accum[j];
            sum /= HOP_SIZE;

            cola_gain_ = 1.0f / sum;
        }

        // --------------------------------------------------------------------
        // Internal State
        // --------------------------------------------------------------------
        Fast_RFFT<FFT_SIZE> fft_;     ///< FFT engine for inverse transform.
        float window_[FFT_SIZE];      ///< Synthesis window.
        float overlap_buf_[FFT_SIZE]; ///< Overlap-add buffer.
        float time_buf_[FFT_SIZE];    ///< Temporary time-domain frame.
        size_t read_pos_; ///< Read position within overlap buffer.
        float cola_gain_; ///< COLA normalization gain.
    };

} // namespace dsp

/* EOF */
