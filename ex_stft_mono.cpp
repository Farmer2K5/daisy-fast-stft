/**
 * @file main_mono.cpp
 * @brief Real-time spectral processing example on the Daisy Seed using daisyfarm::Fast_STFT.
 *
 * @details
 * This example demonstrates a **single-channel (mono)** spectral processing pipeline
 * running on the Daisy Seed. The signal is analyzed and reconstructed in the frequency
 * domain using an overlap-add STFT framework (`Fast_STFT`).
 *
 * - The framework handles buffering, windowing, FFT/IFFT, and overlap-add synthesis.
 * - The audio callback runs continuously, processing `BLOCK_SIZE` samples per call.
 * - A `CpuLoadMeter` monitors performance to ensure real-time stability.
 *
 * @note This example uses **complex-domain** processing mode (no magnitude/phase conversion),
 *       ideal for linear or convolution-type effects.
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

DaisyPod hw; ///< Global Daisy hardware interface

// === STFT Configuration ===
// These parameters must satisfy compile-time constraints enforced by Fast_STFT.
constexpr size_t FFT_SIZE = 1024;                              ///< Size of the FFT window (samples)
constexpr size_t HOP_SIZE = FFT_SIZE / 8;                      ///< Hop size between frames (samples)
constexpr size_t BLOCK_SIZE = 32;                              ///< I/O processing block size (samples)
// constexpr ProcessingMode PROC_MODE = ProcessingMode::Complex;  ///< Complex-domain processing mode
constexpr ProcessingMode PROC_MODE = ProcessingMode::MagPhase; ///< Spectral processing mode (Mag/Phase domain)
constexpr float SAMPLE_RATE = 48000.0f;                        ///< Audio sample rate (Hz)

// -----------------------------------------------------------------------------
// Global DSP Objects
// -----------------------------------------------------------------------------

/**
 * @brief STFT processor instance.
 *
 * @details
 * The Fast_STFT template manages all FFT scheduling, buffering,
 * windowing, and overlap-add reconstruction.
 *
 * - In `ProcessingMode::MagPhase`, spectral frames are converted to magnitude/phase
 *   for custom transformation.
 * - In `ProcessingMode::Complex`, bins are processed directly as complex numbers
 *   (faster, linear-domain operations).
 */
Fast_STFT<FFT_SIZE, HOP_SIZE, BLOCK_SIZE, PROC_MODE> stft;

/** @brief Measures DSP processing load across audio callbacks. */
CpuLoadMeter cpuLoadMeter;

// -----------------------------------------------------------------------------
// Audio Callback
// -----------------------------------------------------------------------------

/**
 * @brief Primary audio processing callback.
 *
 * @param in   Pointer to interleaved input buffers for each channel.
 * @param out  Pointer to interleaved output buffers for each channel.
 * @param size Number of samples per buffer (usually equals `BLOCK_SIZE`).
 *
 * @details
 * - Converts stereo input to mono for processing.
 * - Feeds `BLOCK_SIZE` samples into the STFT processor.
 * - Writes processed mono output to both stereo channels.
 * - Measures CPU time per block via `CpuLoadMeter`.
 *
 * This function executes in the **audio interrupt context**, so avoid
 * dynamic allocation, logging, or heavy math operations here.
 */
void AudioCallback(AudioHandle::InputBuffer in,
                   AudioHandle::OutputBuffer out,
                   size_t size)
{
    cpuLoadMeter.OnBlockStart();

    float mono_in[BLOCK_SIZE];
    float mono_out[BLOCK_SIZE];

    // --- Mix stereo input to mono (use left channel only for simplicity) ---
    for (size_t i = 0; i < size; ++i)
        mono_in[i] = in[0][i];

    // --- Process a block of samples through the STFT pipeline ---
    stft.ProcessAudioBlock(mono_in, mono_out);

    // --- Write processed mono signal back to both stereo outputs ---
    for (size_t i = 0; i < size; ++i)
    {
        out[0][i] = mono_out[i];
        out[1][i] = mono_out[i];
    }

    cpuLoadMeter.OnBlockEnd();
}

// -----------------------------------------------------------------------------
// Main Entry Point
// -----------------------------------------------------------------------------

int main(void)
{
    /** Initialize Daisy hardware and enable CPU boost mode for heavy DSP. */
    const bool boost = true;
    hw.Init(boost);

    /** Initialize USB serial logging for debug output. */
    hw.seed.StartLog(true);

    /** Optional: Override audio sample rate (default = 48 kHz). */
    // hw.SetAudioSampleRate(SAMPLE_RATE);
    const float sample_rate = hw.AudioSampleRate();

    /** Configure the number of samples processed per callback. */
    hw.SetAudioBlockSize(BLOCK_SIZE);

    /** Initialize the CPU load meter for performance monitoring. */
    cpuLoadMeter.Init(sample_rate, BLOCK_SIZE);

    /** Start continuous real-time audio streaming. */
    hw.StartAudio(AudioCallback);

    /** Main loop (non-realtime). Handles periodic logging and housekeeping. */
    while (1)
    {
        // Retrieve current average, minimum, and peak CPU usage
        const float avgLoad = cpuLoadMeter.GetAvgCpuLoad();
        const float maxLoad = cpuLoadMeter.GetMaxCpuLoad();
        const float minLoad = cpuLoadMeter.GetMinCpuLoad();

        // Log CPU load statistics over USB
        hw.seed.PrintLine("Processing Load %%:");
        hw.seed.PrintLine("Max: " FLT_FMT3, FLT_VAR3(maxLoad * 100.0f));
        hw.seed.PrintLine("Avg: " FLT_FMT3, FLT_VAR3(avgLoad * 100.0f));
        hw.seed.PrintLine("Min: " FLT_FMT3, FLT_VAR3(minLoad * 100.0f));

        // Wait 1 second between updates to reduce USB serial spam
        System::Delay(1000);
    }
}
