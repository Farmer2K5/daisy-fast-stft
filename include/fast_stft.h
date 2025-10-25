/**
 * @file fast_sfft.h
 * @brief Block-based Short-Time Fourier Transform (STFT) framework for real-time spectral processing.
 *
 * @details
 * This header defines the `dsp::Fast_STFT` class template — a modular and efficient
 * structure for performing forward and inverse FFT operations using a block-based
 * overlap-add (COLA) approach. It supports both magnitude/phase and complex-domain
 * spectral processing, selectable at compile time.
 *
 * The framework handles circular buffering, windowing, and spectral reconstruction
 * for continuous, low-latency streaming audio or signal data. Derived classes can
 * override either `ProcessFrame()` (for magnitude/phase-domain effects) or
 * `ProcessFrameComplex()` (for direct complex-bin operations).
 *
 * **Key Features:**
 * - Configurable FFT, hop, and block sizes with compile-time validation.
 * - Hann window initialization ensuring perfect COLA reconstruction.
 * - Multiple COLA normalization methods (Max, RMS, Linear — Linear used here).
 * - Circular buffering for real-time block-based signal streaming.
 * - Compile-time selectable spectral processing mode:
 *   - `ProcessingMode::MagPhase` — operates in magnitude/phase domain.
 *   - `ProcessingMode::Complex` — operates on complex FFT bins directly.
 *
 * @tparam kFftSize        FFT size (must be a power of two).
 * @tparam kHopSize        Hop size between frames (must evenly divide FFT size).
 * @tparam kBlockSize      Number of samples per process block (must evenly divide hop size).
 * @tparam kProcessingMode Compile-time processing mode selection (`ProcessingMode::MagPhase` or `ProcessingMode::Complex`).
 *
 * @note Designed for embedded DSP targets such as ARM Cortex-M using CMSIS-DSP.
 *       Suitable for spectral effects, real-time transformations, and time-frequency analysis.
 */

#pragma once
#include <cstring>
#include "arm_math.h"
#include "fast_rfft.h"
#include "fast_spectral.h"
#include "fast_window.h"

namespace dsp
{
    /** @brief Compile-time selectable spectral processing mode. */
    enum class ProcessingMode
    {
        MagPhase, ///< Process using magnitude and phase arrays.
        Complex   ///< Process using raw complex FFT bins.
    };

    /**
     * @class Fast_STFT
     * @brief Template-based STFT processing framework for streaming spectral effects.
     *
     * Provides a reusable and efficient pipeline for spectral audio or signal processing.
     * Handles circular buffering, FFT windowing, overlap-add reconstruction, and
     * spectral-domain processing via user overrides.
     */
    template <size_t kFftSize,
              size_t kHopSize,
              size_t kBlockSize,
              ProcessingMode kProcessingMode = ProcessingMode::MagPhase>
    class Fast_STFT
    {
        static_assert(kFftSize % kHopSize == 0,
                      "FFT_SIZE must be an integer multiple of HOP_SIZE (for COLA).");
        static_assert(kHopSize % kBlockSize == 0,
                      "HOP_SIZE must be an integer multiple of BLOCK_SIZE.");

    public:
        // --------------------------------------------------------------------
        // Compile-time constants
        // --------------------------------------------------------------------
        static constexpr size_t FFT_SIZE = kFftSize;
        static constexpr size_t HOP_SIZE = kHopSize;
        static constexpr size_t BLOCK_SIZE = kBlockSize;
        static constexpr size_t N_BINS = FFT_SIZE / 2 + 1;

        /**
         * @brief Construct and initialize the STFT framework.
         *
         * Initializes the Hann window and computes COLA normalization gain.
         * Clears all internal circular buffers to ensure consistent startup state.
         */
        Fast_STFT()
            : write_idx_(0),
              read_idx_(0),
              accum_(0),
              cola_gain_(1.0f),
              window_type_(WindowType::Hann),
              window_alpha_(0.4f)
        {
            InitWindow();
            ComputeCOLAGain_Linear();

            memset(circ_buf_, 0, sizeof(circ_buf_));
            memset(overlap_buf_, 0, sizeof(overlap_buf_));
            memset(fft_in_, 0, sizeof(fft_in_));
            memset(fft_out_, 0, sizeof(fft_out_));
        }

        virtual ~Fast_STFT() = default;

        // --------------------------------------------------------------------
        // Runtime Window Control
        // --------------------------------------------------------------------

        /**
         * @brief Change window type and reinitialize window + COLA gain.
         * @param type  New window type (Hann, Tukey, Gaussian, etc.)
         * @param alpha Shape parameter (for Gaussian/Tukey)
         */
        void SetWindowType(WindowType type, float alpha = 0.4f)
        {
            window_type_ = type;
            window_alpha_ = alpha;
            InitWindow();
            ComputeCOLAGain_Linear();
        }

        /** @return Currently active window type. */
        WindowType GetWindowType() const { return window_type_; }

        /** @return Current window alpha parameter. */
        float GetWindowAlpha() const { return window_alpha_; }

        // --------------------------------------------------------------------
        // Audio Block Processing
        // --------------------------------------------------------------------

        /**
         * @brief Process a single audio block.
         *
         * Pushes `BLOCK_SIZE` samples into the circular buffer, processes available frames
         * once sufficient samples accumulate, and outputs the next `BLOCK_SIZE` samples
         * after overlap-add synthesis.
         *
         * @param input  Pointer to input sample buffer (size `BLOCK_SIZE`).
         * @param output Pointer to output buffer (size `BLOCK_SIZE`).
         */
        void ProcessAudioBlock(const float *input, float *output)
        {
            // --- Push new samples into circular buffer ---
            for (size_t i = 0; i < BLOCK_SIZE; ++i)
            {
                circ_buf_[write_idx_] = input[i];
                write_idx_ = (write_idx_ + 1) % FFT_SIZE;
            }

            accum_ += BLOCK_SIZE;

            // --- Process frame when enough samples accumulated ---
            while (accum_ >= HOP_SIZE)
            {
                RunFrame();
                accum_ -= HOP_SIZE;
            }

            // --- Output next BLOCK_SIZE samples ---
            for (size_t i = 0; i < BLOCK_SIZE; ++i)
            {
                output[i] = overlap_buf_[read_idx_];
                overlap_buf_[read_idx_] = 0.0f;
                read_idx_ = (read_idx_ + 1) % FFT_SIZE;
            }
        }

    protected:
        // --------------------------------------------------------------------
        // User-Overridable Hooks
        // --------------------------------------------------------------------

        /**
         * @brief User-overridable spectral processing function (magnitude/phase mode).
         *
         * Called after the forward FFT and conversion to magnitude and phase arrays.
         * Override this function to implement spectral-domain effects such as EQ,
         * phase vocoding, or amplitude manipulation.
         *
         * @param mags   Pointer to the array of magnitude values (size `n_bins`).
         * @param phases Pointer to the array of phase values (size `n_bins`).
         * @param n_bins Number of spectral bins (`FFT_SIZE / 2 + 1`).
         */
        virtual void ProcessFrame(float *mags, float *phases, size_t n_bins)
        {
            (void)mags;
            (void)phases;
            (void)n_bins;
        }

        /**
         * @brief User-overridable spectral processing function (complex mode).
         *
         * Called when `ProcessingMode::Complex` is selected at compile time.
         * Operates directly on the interleaved complex FFT bins (real/imag pairs).
         *
         * Override this for convolution, complex-domain filtering, or
         * analytic-signal processing where magnitude/phase decomposition
         * is unnecessary.
         *
         * @param fft_bins Pointer to the FFT output buffer (size `FFT_SIZE`).
         * @param n_bins   Number of spectral bins (`FFT_SIZE / 2 + 1`).
         */
        virtual void ProcessFrameComplex(float *fft_bins, size_t n_bins)
        {
            (void)fft_bins;
            (void)n_bins;
        }

    private:
        // --------------------------------------------------------------------
        // Windowing and Gain Compensation
        // --------------------------------------------------------------------

        /**
         * @brief Initialize the Hann window used for analysis/synthesis.
         *
         * @details
         * Generates a symmetric Hann window (`0.5 * (1 - cos(2πn/N))`)
         * that satisfies the COLA condition for perfect overlap-add
         * reconstruction given appropriate hop size.
         */
        void InitWindow()
        {
            dsp::MakeWindow(dsp::WindowType::Hann, window_, FFT_SIZE);
            // No normalization — COLA gain handles amplitude correction.
        }

        /**
         * @brief Compute linear-mean COLA normalization gain.
         *
         * @details
         * Ensures amplitude consistency across overlapping windows by
         * computing the average overlap energy of squared window samples.
         * This version uses linear mean normalization, which avoids buzz
         * artifacts sometimes present in RMS-based methods.
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
        // FFT Frame Processing Core
        // --------------------------------------------------------------------

        /**
         * @brief Perform a full STFT frame processing cycle.
         *
         * Executes the forward FFT, user-defined spectral processing
         * (in magnitude/phase or complex domain), inverse FFT, and
         * overlap-add synthesis.
         */
        void RunFrame()
        {
            // 1. Gather FFT_SIZE samples and apply window
            for (size_t i = 0; i < FFT_SIZE; ++i)
            {
                size_t idx = (write_idx_ + i) % FFT_SIZE;
                fft_in_[i] = circ_buf_[idx];
            }
            dsp::ApplyWindow(window_, fft_in_, FFT_SIZE);

            // 2. Forward FFT
            fft_.Forward(fft_in_, fft_out_);

            // 3. Compile-time spectral processing path
            if constexpr (kProcessingMode == ProcessingMode::MagPhase)
            {
                dsp::ToMagPhase(fft_out_, mags_, phases_, FFT_SIZE);
                ProcessFrame(mags_, phases_, N_BINS);
                dsp::FromMagPhase(mags_, phases_, fft_out_, FFT_SIZE);
            }
            else // ProcessingMode::Complex
            {
                ProcessFrameComplex(fft_out_, N_BINS);
            }

            // 4. Inverse FFT
            fft_.Inverse(fft_out_, fft_in_);

            // 5. Overlap-add synthesis with COLA gain
            for (size_t i = 0; i < FFT_SIZE; ++i)
            {
                size_t idx = (read_idx_ + i) % FFT_SIZE;
                overlap_buf_[idx] += fft_in_[i] * window_[i] * cola_gain_;
            }
        }

    private:
        // --------------------------------------------------------------------
        // Internal State
        // --------------------------------------------------------------------
        Fast_RFFT<FFT_SIZE> fft_; ///< Underlying real FFT processor.

        float circ_buf_[FFT_SIZE];    ///< Circular input buffer.
        float overlap_buf_[FFT_SIZE]; ///< Overlap-add buffer for output accumulation.
        float window_[FFT_SIZE];      ///< Hann window coefficients.
        float fft_in_[FFT_SIZE];      ///< Time-domain FFT input buffer.
        float fft_out_[FFT_SIZE];     ///< Frequency-domain FFT output buffer.
        float mags_[N_BINS];          ///< Magnitude spectrum buffer.
        float phases_[N_BINS];        ///< Phase spectrum buffer.

        size_t write_idx_; ///< Write index in circular buffer.
        size_t read_idx_;  ///< Read index in overlap buffer.
        size_t accum_;     ///< Accumulated block count toward next FFT frame.
        float cola_gain_;  ///< Normalization gain for COLA synthesis.

        WindowType window_type_;
        float window_alpha_;
    };

} // namespace dsp

/* EOF */
