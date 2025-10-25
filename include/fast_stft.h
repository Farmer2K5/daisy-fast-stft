/**
 * @file fast_stft.h
 * @brief Short-Time Fourier Transform (STFT) framework for real-time spectral audio processing.
 *
 * @details
 * The `Fast_STFT` class template provides a modular, real-time implementation of the
 * Short-Time Fourier Transform (STFT) suitable for embedded audio systems.
 * It handles circular buffering, FFT-based spectral conversion, windowing,
 * and overlap-add (COLA) reconstruction for continuous signal streaming.
 *
 * **Core Features:**
 * - Configurable FFT, hop, and block sizes with compile-time validation.
 * - Integrated CMSIS-DSP real FFT (`Fast_RFFT`) for maximum speed.
 * - Seamless window handling using `Fast_Window` utilities.
 * - Compile-time selectable spectral processing mode:
 *   - `ProcessingMode::MagPhase` — process in magnitude/phase domain.
 *   - `ProcessingMode::Complex` — process directly on complex FFT bins.
 * - COLA normalization for perfect amplitude reconstruction.
 *
 * **Use Cases:**
 * - Real-time spectral effects (EQ, pitch shift, vocoding)
 * - Spectral feature extraction
 * - Phase vocoders or resynthesis engines
 *
 * @note
 * Designed for high-performance embedded audio environments such as
 * ARM Cortex-M7 (Daisy Seed, STM32H7) using CMSIS-DSP intrinsics.
 *
 * @ingroup FastDSP
 * @defgroup FastSTFT Short-Time Fourier Transform (STFT)
 * @ingroup FastDSP
 * @brief Real-time block-based spectral processing framework.
 */

#pragma once
#include <cstring>
#include "arm_math.h"
#include "fast_rfft.h"
#include "fast_spectral.h"
#include "fast_window.h"

namespace dsp
{
    /**
     * @enum ProcessingMode
     * @brief Selects which spectral representation the STFT will process.
     *
     * @details
     * - `MagPhase`: Converts FFT data to magnitude/phase arrays for perceptually
     *   meaningful operations (e.g., spectral shaping, morphing, dynamics).
     * - `Complex`: Operates directly on the raw complex FFT bins for low-level
     *   effects such as convolution, Hilbert-domain modulation, or custom phase manipulation.
     */
    enum class ProcessingMode
    {
        MagPhase, ///< Process magnitude and phase separately.
        Complex   ///< Process FFT complex bins directly.
    };

    /**
     * @class Fast_STFT
     * @brief Real-time Short-Time Fourier Transform (STFT) processor template.
     *
     * @tparam kFftSize        FFT size in samples (must be power of two).
     * @tparam kHopSize        Hop size between frames (must divide `FFT_SIZE`).
     * @tparam kBlockSize      Block size of audio processing buffer (must divide `HOP_SIZE`).
     * @tparam kProcessingMode Processing mode (`MagPhase` or `Complex`).
     *
     * @details
     * Provides a reusable streaming STFT engine designed for low-latency
     * spectral processing. Internally, it manages:
     * - Circular I/O buffers
     * - Windowing (via `Fast_Window`)
     * - FFT and inverse FFT
     * - Overlap-add synthesis with COLA normalization
     *
     * Users subclass this template and override one of:
     * - `ProcessFrame()` — to modify magnitude/phase spectra
     * - `ProcessFrameComplex()` — to modify complex bins directly
     *
     * @par Example
     * @code
     * class MySpectralEffect : public dsp::Fast_STFT<1024, 256, 64>
     * {
     * protected:
     *     void ProcessFrame(float *mags, float *phases, size_t n_bins) override
     *     {
     *         for(size_t i = 0; i < n_bins; i++)
     *             mags[i] *= 0.8f; // simple spectral attenuation
     *     }
     * };
     * @endcode
     */
    template <size_t kFftSize,
              size_t kHopSize,
              size_t kBlockSize,
              ProcessingMode kProcessingMode = ProcessingMode::MagPhase>
    class Fast_STFT
    {
        static_assert(kFftSize % kHopSize == 0,
                      "FFT_SIZE must be an integer multiple of HOP_SIZE for COLA consistency.");
        static_assert(kHopSize % kBlockSize == 0,
                      "HOP_SIZE must be an integer multiple of BLOCK_SIZE for block alignment.");

    public:
        // --------------------------------------------------------------------
        // Compile-Time Constants
        // --------------------------------------------------------------------
        static constexpr size_t FFT_SIZE = kFftSize;       ///< FFT length in samples.
        static constexpr size_t HOP_SIZE = kHopSize;       ///< Hop size between analysis frames.
        static constexpr size_t BLOCK_SIZE = kBlockSize;   ///< Number of samples processed per audio callback.
        static constexpr size_t N_BINS = FFT_SIZE / 2 + 1; ///< Number of FFT magnitude bins.

        /**
         * @brief Construct a new Fast_STFT object.
         *
         * Initializes window coefficients, COLA gain, and all internal buffers.
         * By default, a Hann window is used with `alpha = 0.4`.
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
        // Window Configuration
        // --------------------------------------------------------------------

        /**
         * @brief Change the analysis/synthesis window at runtime.
         *
         * @param type  Window function (Hann, Hamming, Blackman, Gaussian, etc.)
         * @param alpha Optional shape parameter (Tukey/Gaussian width)
         *
         * Reinitializes the window and recomputes COLA gain to maintain amplitude
         * consistency. Matching analysis/synthesis windows are required for
         * perfect reconstruction.
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

        /** @return Current alpha shape parameter. */
        float GetWindowAlpha() const { return window_alpha_; }

        // --------------------------------------------------------------------
        // Audio Block Processing
        // --------------------------------------------------------------------

        /**
         * @brief Process one audio block (analysis + synthesis).
         *
         * @param input  Input audio block (`BLOCK_SIZE` samples).
         * @param output Output buffer (`BLOCK_SIZE` samples).
         *
         * @details
         * The function:
         *  1. Pushes samples into a circular buffer.
         *  2. Triggers a new STFT frame when `HOP_SIZE` samples have accumulated.
         *  3. Processes spectral data via user override.
         *  4. Performs inverse FFT and overlap-add synthesis.
         *
         * This is designed for use inside a real-time audio callback loop.
         */
        void ProcessAudioBlock(const float *input, float *output)
        {
            // --- Push input samples into circular buffer ---
            for (size_t i = 0; i < BLOCK_SIZE; ++i)
            {
                circ_buf_[write_idx_] = input[i];
                write_idx_ = (write_idx_ + 1) % FFT_SIZE;
            }

            accum_ += BLOCK_SIZE;

            // --- Process new frame when enough samples accumulated ---
            while (accum_ >= HOP_SIZE)
            {
                RunFrame();
                accum_ -= HOP_SIZE;
            }

            // --- Retrieve synthesized output ---
            for (size_t i = 0; i < BLOCK_SIZE; ++i)
            {
                output[i] = overlap_buf_[read_idx_];
                overlap_buf_[read_idx_] = 0.0f;
                read_idx_ = (read_idx_ + 1) % FFT_SIZE;
            }
        }

    protected:
        // --------------------------------------------------------------------
        // User-Overridable Processing Hooks
        // --------------------------------------------------------------------

        /**
         * @brief Override to implement spectral-domain effects (magnitude/phase mode).
         *
         * @param mags   Magnitude array (size `N_BINS`)
         * @param phases Phase array (size `N_BINS`)
         * @param n_bins Number of bins (FFT_SIZE / 2 + 1)
         *
         * @note
         * Only called when `ProcessingMode::MagPhase` is selected.
         * Default implementation is a no-op.
         */
        virtual void ProcessFrame(float *mags, float *phases, size_t n_bins)
        {
            (void)mags;
            (void)phases;
            (void)n_bins;
        }

        /**
         * @brief Override to implement effects directly in the complex domain.
         *
         * @param fft_bins Pointer to FFT output buffer (interleaved Re/Im)
         * @param n_bins   Number of spectral bins (`N_BINS`)
         *
         * @note
         * Only called when `ProcessingMode::Complex` is selected.
         */
        virtual void ProcessFrameComplex(float *fft_bins, size_t n_bins)
        {
            (void)fft_bins;
            (void)n_bins;
        }

    private:
        // --------------------------------------------------------------------
        // Internal Helpers: Window & COLA Gain
        // --------------------------------------------------------------------

        /** @brief Initialize and fill window coefficients. */
        void InitWindow()
        {
            dsp::MakeWindow(window_type_, window_, FFT_SIZE, window_alpha_);
        }

        /**
         * @brief Compute constant overlap-add (COLA) gain correction.
         *
         * @details
         * Ensures linear reconstruction gain when overlapping windows are summed.
         * This method uses a *linear average* of squared window samples over the
         * hop interval, providing smoother energy balance than RMS normalization.
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
        // Internal Core: Frame Processing Cycle
        // --------------------------------------------------------------------

        /**
         * @brief Execute a full STFT frame cycle: analysis, processing, synthesis.
         *
         * Steps:
         *  1. Gather and window `FFT_SIZE` samples.
         *  2. Perform forward FFT.
         *  3. Run user-defined spectral processing.
         *  4. Inverse FFT and overlap-add synthesis.
         */
        void RunFrame()
        {
            // --- 1. Gather FFT_SIZE samples ---
            for (size_t i = 0; i < FFT_SIZE; ++i)
            {
                size_t idx = (write_idx_ + i) % FFT_SIZE;
                fft_in_[i] = circ_buf_[idx];
            }
            dsp::ApplyWindow(window_, fft_in_, FFT_SIZE);

            // --- 2. Forward FFT ---
            fft_.Forward(fft_in_, fft_out_);

            // --- 3. User-defined spectral processing ---
            if constexpr (kProcessingMode == ProcessingMode::MagPhase)
            {
                dsp::ToMagPhase(fft_out_, mags_, phases_, FFT_SIZE);
                ProcessFrame(mags_, phases_, N_BINS);
                dsp::FromMagPhase(mags_, phases_, fft_out_, FFT_SIZE);
            }
            else
            {
                ProcessFrameComplex(fft_out_, N_BINS);
            }

            // --- 4. Inverse FFT ---
            fft_.Inverse(fft_out_, fft_in_);

            // --- 5. Overlap-add synthesis ---
            for (size_t i = 0; i < FFT_SIZE; ++i)
            {
                size_t idx = (read_idx_ + i) % FFT_SIZE;
                overlap_buf_[idx] += fft_in_[i] * window_[i] * cola_gain_;
            }
        }

        // --------------------------------------------------------------------
        // Internal State Variables
        // --------------------------------------------------------------------
        Fast_RFFT<FFT_SIZE> fft_;     ///< FFT processor instance.
        float circ_buf_[FFT_SIZE];    ///< Input circular buffer.
        float overlap_buf_[FFT_SIZE]; ///< Output accumulation buffer.
        float window_[FFT_SIZE];      ///< Analysis/synthesis window.
        float fft_in_[FFT_SIZE];      ///< FFT input buffer.
        float fft_out_[FFT_SIZE];     ///< FFT output buffer.
        float mags_[N_BINS];          ///< Magnitude spectrum buffer.
        float phases_[N_BINS];        ///< Phase spectrum buffer.
        size_t write_idx_;            ///< Write index for circular buffer.
        size_t read_idx_;             ///< Read index for output buffer.
        size_t accum_;                ///< Number of accumulated samples toward next frame.
        float cola_gain_;             ///< Constant overlap-add normalization factor.
        WindowType window_type_;      ///< Current window function type.
        float window_alpha_;          ///< Window shape parameter.
    };

} // namespace dsp

/* EOF */
