/**
 * @file main_stereo.cpp
 * @brief Stereo real-time spectral processing on the Daisy Seed using daisyfarm::Fast_STFT.
 *
 * @details
 * This example demonstrates a dual-channel (stereo) spectral processing pipeline
 * using the `daisyfarm::Fast_STFT` class on the Daisy Seed.
 *
 * Each channel (Left and Right) runs its own STFT analysis/synthesis chain using
 * separate framework instances (`sfft_L` and `sfft_R`).
 *
 * **Processing flow:**
 *  - Each audio block (BLOCK_SIZE samples) is analyzed using an FFT of size FFT_SIZE.
 *  - The spectral data is optionally modified by user-defined `ProcessFrame()` logic.
 *  - The inverse FFT reconstructs the signal with COLA overlap-add synthesis.
 *  - Output samples are written back to stereo DAC channels.
 *
 * @note
 * This example uses `ProcessingMode::MagPhase` (magnitude/phase processing),
 * which decomposes each FFT frame for easy manipulation of amplitude and phase
 * spectra (e.g., EQ, pitch shifting, or spectral gating).
 */

#include "daisy_pod.h"
#include "daisysp.h"
#include "fast_stft.h"

using namespace daisy;
using namespace daisysp;
using namespace daisyfarm;

// -----------------------------------------------------------------------------
// Hardware + DSP Configuration
// -----------------------------------------------------------------------------

DaisyPod hw; ///< Global Daisy hardware object

// === STFT Configuration ===
// Each parameter must satisfy compile-time constraints enforced by Fast_STFT.
constexpr size_t FFT_SIZE = 1024;                             ///< FFT window size (samples)
constexpr size_t HOP_SIZE = FFT_SIZE / 4;                     ///< Hop size between analysis frames (samples)
constexpr size_t BLOCK_SIZE = 64;                             ///< Audio processing block size (samples)
// constexpr ProcessingMode PROC_MODE = ProcessingMode::Complex; ///< Complex-domain processing mode
constexpr ProcessingMode PROC_MODE = ProcessingMode::MagPhase; ///< Spectral processing mode (Mag/Phase domain)
constexpr float SAMPLE_RATE = 48000.0f; ///< System audio sample rate (Hz)

// -----------------------------------------------------------------------------
// Global DSP Objects
// -----------------------------------------------------------------------------

/**
 * @brief Left and Right STFT processors.
 *
 * @details
 * Each instance of Fast_STFT handles buffering, windowing, FFT computation,
 * spectral processing, and overlap-add synthesis for a single audio channel.
 *
 * For stereo processing, two independent instances are required to preserve
 * channel separation and avoid crosstalk in the FFT buffers.
 */
Fast_STFT<FFT_SIZE, HOP_SIZE, BLOCK_SIZE, PROC_MODE> sfft_L;
Fast_STFT<FFT_SIZE, HOP_SIZE, BLOCK_SIZE, PROC_MODE> sfft_R;

/** @brief Measures the processing load of the audio callback in real time. */
CpuLoadMeter cpuLoadMeter;

// -----------------------------------------------------------------------------
// Audio Callback
// -----------------------------------------------------------------------------

/**
 * @brief Real-time audio callback for stereo spectral processing.
 *
 * @param in   Interleaved input buffers for stereo channels.
 * @param out  Interleaved output buffers for stereo channels.
 * @param size Number of samples per audio block (typically == BLOCK_SIZE).
 *
 * @details
 * - Processes left and right channels independently through their respective
 *   STFT pipelines (`sfft_L` and `sfft_R`).
 * - The callback operates in the audio interrupt context, so any spectral
 *   processing must be real-time safe.
 * - CPU usage is measured per block for performance monitoring.
 */
void AudioCallback(AudioHandle::InputBuffer in,
                   AudioHandle::OutputBuffer out,
                   size_t size)
{
    cpuLoadMeter.OnBlockStart();

    // --- Process each channel independently ---
    sfft_L.ProcessAudioBlock(in[0], out[0]);
    sfft_R.ProcessAudioBlock(in[1], out[1]);

    cpuLoadMeter.OnBlockEnd();
}

// -----------------------------------------------------------------------------
// Main Entry Point
// -----------------------------------------------------------------------------

int main(void)
{
    /** Initialize the Daisy hardware and enable high-performance mode. */
    const bool boost = true;
    hw.Init(boost);

    /** Start USB serial logging for runtime diagnostics. */
    hw.seed.StartLog();

    /** Optional: Set a custom audio sample rate (default is 48 kHz). */
    // hw.SetAudioSampleRate(SAMPLE_RATE);
    const float sample_rate = hw.AudioSampleRate();

    /** Set the size of audio blocks processed in each callback. */
    hw.SetAudioBlockSize(BLOCK_SIZE);

    /** Initialize CPU usage meter for monitoring real-time performance. */
    cpuLoadMeter.Init(sample_rate, BLOCK_SIZE);

    /** Start continuous real-time audio streaming via the callback. */
    hw.StartAudio(AudioCallback);

    /** Idle loop — real-time audio runs in interrupt context. */
    while (1)
    {
        // Retrieve current CPU usage metrics
        const float avgLoad = cpuLoadMeter.GetAvgCpuLoad();
        const float maxLoad = cpuLoadMeter.GetMaxCpuLoad();
        const float minLoad = cpuLoadMeter.GetMinCpuLoad();

        // Print CPU usage over USB serial (as percentages)
        hw.seed.PrintLine("Processing Load %%:");
        hw.seed.PrintLine("Max: " FLT_FMT3, FLT_VAR3(maxLoad * 100.0f));
        hw.seed.PrintLine("Avg: " FLT_FMT3, FLT_VAR3(avgLoad * 100.0f));
        hw.seed.PrintLine("Min: " FLT_FMT3, FLT_VAR3(minLoad * 100.0f));

        // Wait 1 second between reports to avoid spamming the serial connection
        System::Delay(1000);
    }
}
