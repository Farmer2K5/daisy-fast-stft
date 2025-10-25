/**
 * @file fast_istft.h
 * @brief Inverse Short-Time Fourier Transform (ISTFT) framework.
 *
 * @details
 * This class reconstructs a time-domain audio signal from spectral (FFT)
 * frames produced by an STFT process. It performs:
 *  - Windowed inverse FFT (via `Fast_RFFT`)
 *  - Overlap-add accumulation for continuous output
 *  - Constant Overlap-Add (COLA) normalization for perfect amplitude
 *
 * It is designed to pair with:
 *  - `Fast_STFT` (for real-time analysis and synthesis)
 *  - `Fast_RFFT` (FFT backend)
 *  - `Fast_Window` (windowing utilities)
 *
 * While `Fast_STFT` handles both analysis and synthesis for real-time DSP,
 * this class is useful for:
 *  - **Offline reconstruction** (e.g., spectral editing or rendering)
 *  - **Modular DSP architectures** with separate analysis/synthesis blocks
 *  - **Machine-learning audio models** operating in the spectral domain
 *
 * @ingroup FastDSP
 * @defgroup FastISTFT Inverse Short-Time Fourier Transform (ISTFT)
 * @ingroup FastDSP
 * @brief Windowed overlap-add resynthesis of audio from spectral frames.
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
     * @tparam kFftSize   FFT size (must match the analysis FFT size).
     * @tparam kHopSize   Hop size (must match the analysis hop interval).
     *
     * @details
     * The ISTFT takes successive spectral frames (real/imag pairs or magnitudes/phases)
     * and reconstructs continuous time-domain audio via windowed inverse FFT and
     * overlap-add synthesis.
     *
     * - **FFT size** defines frequency resolution.
     * - **Hop size** defines time resolution and overlap ratio.
     * - **COLA normalization** ensures correct amplitude reconstruction.
     *
     * Typical use case:
     * @code
     * dsp::Fast_ISTFT<1024, 256> istft;
     * float output[256];
     * istft.ProcessFrame(fft_data, output);
     * @endcode
     *
     * @note
     * This class assumes the spectral frames have been generated using a compatible
     * `Fast_STFT` or external analysis process with matching parameters.
     */
    template <size_t kFftSize, size_t kHopSize>
    class Fast_ISTFT
    {
    public:
        /** @brief FFT size in samples. */
        static constexpr size_t FFT_SIZE = kFftSize;

        /** @brief Hop size (number of samples advanced between frames). */
        static constexpr size_t HOP_SIZE = kHopSize;

        /** @brief Number of unique complex bins in a real-valued FFT. */
        static constexpr size_t N_BINS = FFT_SIZE / 2 + 1;

        /**
         * @brief Construct a new Fast_ISTFT object.
         *
         * Initializes synthesis window, overlap buffer, and COLA gain factor.
         * The default synthesis window is **Hann**, which guarantees perfect
         * COLA behavior when the hop size is `FFT_SIZE / 4` (25% overlap).
         *
         * @see SetWindow()
         * @see ComputeCOLAGain_Linear()
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
         * @details
         * Clears all accumulated data and resets internal state. This is useful
         * when restarting the ISTFT (e.g., for a new audio stream or segment).
         *
         * @warning
         * Call this before processing a new unrelated signal to avoid residual overlap.
         */
        void Reset()
        {
            std::memset(overlap_buf_, 0, sizeof(overlap_buf_));
            read_pos_ = 0;
        }

        /**
         * @brief Set a different synthesis window type.
         *
         * @param type  Window function type (Hann, Hamming, Gaussian, Tukey, etc.).
         * @param alpha Shape parameter for adjustable windows (Tukey/Gaussian).
         *
         * @details
         * Updates the synthesis window and recomputes the COLA normalization gain.
         * The same window type and hop size used in analysis should typically be
         * used for accurate reconstruction.
         *
         * @note
         * Common window choices:
         * - **Hann** → Perfect reconstruction (COLA)
         * - **Gaussian** → Smooth spectral interpolation
         * - **Tukey** → Adjustable time-frequency balance
         */
        void SetWindow(WindowType type, float alpha = 0.4f)
        {
            dsp::MakeWindow(type, window_, FFT_SIZE, alpha);
            ComputeCOLAGain_Linear();
        }

        /**
         * @brief Process one inverse STFT frame.
         *
         * @param fft_data Input FFT data (interleaved real/imag pairs).
         * @param output   Output time-domain buffer (length = `HOP_SIZE`).
         *
         * @details
         * This method performs the following sequence:
         *  1. **Inverse FFT** — Converts frequency-domain data to time domain.
         *  2. **Apply window** — Restores the synthesis envelope.
         *  3. **Overlap-add** — Adds the windowed frame into a rolling buffer.
         *  4. **Output hop** — Returns the next hop-sized segment.
         *
         * The overlap-add buffer ensures smooth continuity between frames
         * and allows for seamless resynthesis without discontinuities.
         *
         * @note
         * - `fft_data` must use the same packed format as CMSIS-DSP RFFT.
         * - The function assumes consistent frame-to-frame phase progression.
         */
        void ProcessFrame(float *fft_data, float *output)
        {
            // (1) Inverse FFT → time-domain buffer
            fft_.Inverse(fft_data, time_buf_);

            // (2) Apply synthesis window
            dsp::ApplyWindow(window_, time_buf_, FFT_SIZE);

            // (3) Overlap-add accumulation into circular buffer
            for (size_t i = 0; i < FFT_SIZE; ++i)
            {
                size_t idx = (read_pos_ + i) % FFT_SIZE;
                overlap_buf_[idx] += time_buf_[i] * cola_gain_;
            }

            // (4) Output hop-sized segment
            for (size_t i = 0; i < HOP_SIZE; ++i)
            {
                output[i] = overlap_buf_[(read_pos_ + i) % FFT_SIZE];
                overlap_buf_[(read_pos_ + i) % FFT_SIZE] = 0.0f;
            }

            // (5) Advance read position
            read_pos_ = (read_pos_ + HOP_SIZE) % FFT_SIZE;
        }

    private:
        /**
         * @brief Compute constant overlap-add (COLA) normalization factor.
         *
         * @details
         * The COLA gain ensures that overlapping windowed frames sum to
         * unity gain in the reconstructed signal. It computes:
         * \f[
         * G = \frac{1}{\text{mean}\left(\sum_i w^2[n + i \cdot H]\right)}
         * \f]
         * where \(H\) is the hop size.
         *
         * @note
         * This function assumes that `FFT_SIZE` is a multiple of `HOP_SIZE`.
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
        Fast_RFFT<FFT_SIZE> fft_;     ///< FFT engine used for inverse transform.
        float window_[FFT_SIZE];      ///< Synthesis window coefficients.
        float overlap_buf_[FFT_SIZE]; ///< Circular overlap-add buffer.
        float time_buf_[FFT_SIZE];    ///< Temporary frame buffer for IFFT output.
        size_t read_pos_;             ///< Read pointer within overlap buffer.
        float cola_gain_;             ///< Constant overlap-add normalization gain.
    };

} // namespace dsp

/* EOF */
