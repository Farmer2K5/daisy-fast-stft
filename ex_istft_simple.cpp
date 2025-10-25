#include "daisy_pod.h"
#include "daisysp.h"
#include "fast_rfft.h"
#include "fast_window.h"
#include "fast_istft.h"

using namespace daisy;
using namespace dsp;

// -----------------------------------------------------------------------------
// Constants
// -----------------------------------------------------------------------------
constexpr size_t FFT_SIZE = 1024;
constexpr size_t HOP_SIZE = 256;
constexpr size_t N_BINS = FFT_SIZE / 2 + 1;
constexpr float SAMPLE_RATE = 48000.0f;

// -----------------------------------------------------------------------------
// Hardware + DSP Globals
// -----------------------------------------------------------------------------
DaisyPod hw;
Fast_ISTFT<FFT_SIZE, HOP_SIZE> istft;
float fft_buf[FFT_SIZE];   // interleaved real/imag buffer
float out_block[HOP_SIZE]; // ISTFT output
float phase_accum[N_BINS]; // per-bin phase accumulator
size_t frame_pos = 0;      // position inside current hop
float base_freq = 110.0f;  // fundamental (Hz)
float time_step = 1.0f / SAMPLE_RATE;

// -----------------------------------------------------------------------------
// Helper: build one spectral frame (additive partials → FFT bins)
// -----------------------------------------------------------------------------
void GenerateSpectralFrame(float time)
{
    // Clear buffer
    for (size_t i = 0; i < FFT_SIZE; i++)
        fft_buf[i] = 0.0f;

    // Build a few harmonics as sinusoids in the frequency domain
    for (size_t k = 1; k < 12; ++k)
    {
        float freq = base_freq * k;
        if (freq > SAMPLE_RATE / 2.0f)
            break;

        size_t bin = static_cast<size_t>((freq / SAMPLE_RATE) * FFT_SIZE);
        float mag = 0.05f / k; // amplitude falloff
        float phase = phase_accum[bin];

        // Real and imaginary components
        fft_buf[2 * bin + 0] = mag * cosf(phase);
        fft_buf[2 * bin + 1] = mag * sinf(phase);

        // Update running phase for smooth continuity
        phase_accum[bin] += 2.0f * M_PI * freq * time_step * HOP_SIZE;
        if (phase_accum[bin] > 2.0f * M_PI)
            phase_accum[bin] -= 2.0f * M_PI;
    }
}

// -----------------------------------------------------------------------------
// Audio callback
// -----------------------------------------------------------------------------
void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
    for (size_t i = 0; i < size; ++i)
    {
        // Render new spectral frame every hop
        if (frame_pos == 0)
        {
            float t = hw.AudioSampleRate() * time_step;
            GenerateSpectralFrame(t);
            istft.ProcessFrame(fft_buf, out_block);
        }

        // Output next sample from overlap buffer
        float s = out_block[frame_pos++];
        if (frame_pos >= HOP_SIZE)
            frame_pos = 0;

        // Stereo out
        out[0][i] = s;
        out[1][i] = s;
    }
}

// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------
int main(void)
{
    hw.Init();
    hw.SetAudioBlockSize(48); // 1 ms
    hw.seed.StartLog();
    hw.StartAudio(AudioCallback);

    // Optional: Gaussian window for smooth tone
    istft.SetWindow(WindowType::Gaussian, 0.5f);

    // Clear phases
    for (auto &p : phase_accum)
        p = 0.0f;

    hw.seed. PrintLine("Spectral Pad Synth running...");
    while (1)
    {
        System::Delay(100);
    }
}
