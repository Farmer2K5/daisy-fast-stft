#include "daisy_pod.h"
#include "daisysp.h"
#include "fast_rfft.h"
#include "fast_window.h"
#include "fast_istft.h"

using namespace daisy;
using namespace daisyfarm;

constexpr size_t FFT_SIZE = 1024;
constexpr size_t HOP_SIZE = 256;
constexpr size_t N_BINS   = FFT_SIZE / 2 + 1;
constexpr float SAMPLE_RATE = 48000.0f;

DaisyPod hw;
Fast_ISTFT<FFT_SIZE, HOP_SIZE> istft;
float fft_buf[FFT_SIZE];
float out_block[HOP_SIZE];
float phase_accum[N_BINS];
size_t frame_pos = 0;
float base_freq = 220.0f;
float global_time = 0.0f;

void GenerateSpectralFrame(float time)
{
    for (size_t i = 0; i < FFT_SIZE; i++)
        fft_buf[i] = 0.0f;

    for (size_t k = 1; k < 12; ++k)
    {
        float freq = base_freq * k * (1.0f + 0.001f * sinf(0.05f * time + k));
        if (freq >= SAMPLE_RATE / 2.0f)
            break;

        size_t bin = static_cast<size_t>((freq / SAMPLE_RATE) * FFT_SIZE);
        float mag = (0.4f / k) * (0.7f + 0.3f * sinf(0.2f * time + 0.3f * k));
        float phase = phase_accum[bin];

        fft_buf[2 * bin + 0] = mag * cosf(phase);
        fft_buf[2 * bin + 1] = mag * sinf(phase);

        phase_accum[bin] += 2.0f * M_PI * freq / SAMPLE_RATE * HOP_SIZE;
        if (phase_accum[bin] > 2.0f * M_PI)
            phase_accum[bin] -= 2.0f * M_PI;
    }

    fft_buf[0] = 0.0f;
    fft_buf[1] = 0.0f;
}

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
    for (size_t i = 0; i < size; i++)
    {
        if (frame_pos == 0)
        {
            GenerateSpectralFrame(global_time);
            istft.ProcessFrame(fft_buf, out_block);
            global_time += static_cast<float>(HOP_SIZE) / SAMPLE_RATE;
        }

        float s = out_block[frame_pos++];
        if (frame_pos >= HOP_SIZE)
            frame_pos = 0;

        out[0][i] = s;
        out[1][i] = s;
    }
}

int main(void)
{
    hw.Init();
    hw.SetAudioBlockSize(48);
    istft.SetWindow(WindowType::Hann);
    for (auto &p : phase_accum)
        p = 0.0f;
    hw.StartAudio(AudioCallback);
    while (1) System::Delay(100);
}