/**
 * @file fast_spectral_features.h
 * @brief Feature extraction utilities for spectral analysis.
 *
 * @details
 * Provides a collection of analysis functions for computing scalar features
 * from FFT magnitude spectra, such as centroid, spread, rolloff, and flux.
 *
 * These features can be used for:
 * - Audio classification and visualization
 * - Adaptive effects (spectral balance, tonal control)
 * - Machine learning feature extraction
 * - General-purpose spectral analysis
 *
 * All functions operate on **magnitude spectra** (`|X[k]|`) computed from real FFTs
 * or STFT frames using the FastDSP framework.
 *
 * @ingroup FastDSPSpectral
 * @defgroup FastDSPSpectralFeatures Spectral Feature Extraction
 * @brief Compute scalar descriptors such as centroid, spread, flux, and entropy.
 */

#pragma once
#include <cmath>
#include <cstddef>

namespace dsp::spectral::features
{

    // -----------------------------------------------------------------------------
    // 1. Core Spectral Features
    // -----------------------------------------------------------------------------

    /**
     * @brief Compute the spectral centroid (center of mass of the spectrum).
     *
     * @param[in] mag    Magnitude spectrum array (length = N/2 + 1).
     * @param[in] freqs  Frequency bin centers (Hz, length = N/2 + 1).
     * @param[in] n_bins Number of bins (`fft_size / 2 + 1`).
     * @return Spectral centroid frequency in Hz.
     *
     * @details
     * \f[
     * C = \frac{\sum_k f_k |X[k]|}{\sum_k |X[k]|}
     * \f]
     */
    inline float SpectralCentroid(const float *mag, const float *freqs, size_t n_bins)
    {
        float num = 0.0f, den = 0.0f;
        for (size_t k = 0; k < n_bins; ++k)
        {
            num += freqs[k] * mag[k];
            den += mag[k];
        }
        return (den > 0.0f) ? num / den : 0.0f;
    }

    /**
     * @brief Compute spectral spread (variance around the centroid).
     *
     * @param[in] mag     Magnitude spectrum (length = N/2 + 1).
     * @param[in] freqs   Frequency bin centers (Hz, length = N/2 + 1).
     * @param[in] n_bins  Number of bins.
     * @param[in] centroid Previously computed spectral centroid.
     * @return Spread in Hz (standard deviation of spectral energy).
     */
    inline float SpectralSpread(const float *mag, const float *freqs, size_t n_bins, float centroid)
    {
        float num = 0.0f, den = 0.0f;
        for (size_t k = 0; k < n_bins; ++k)
        {
            float diff = freqs[k] - centroid;
            num += mag[k] * diff * diff;
            den += mag[k];
        }
        return (den > 0.0f) ? sqrtf(num / den) : 0.0f;
    }

    /**
     * @brief Compute spectral flux between two successive magnitude spectra.
     *
     * @param[in] curr_mag Current magnitude spectrum (length = N/2 + 1).
     * @param[in] prev_mag Previous magnitude spectrum (length = N/2 + 1).
     * @param[in] n_bins   Number of bins.
     * @return Spectral flux (sum of squared magnitude differences).
     */
    inline float SpectralFlux(const float *curr_mag, const float *prev_mag, size_t n_bins)
    {
        float flux = 0.0f;
        for (size_t k = 0; k < n_bins; ++k)
        {
            float diff = curr_mag[k] - prev_mag[k];
            flux += diff * diff;
        }
        return flux / static_cast<float>(n_bins);
    }

    /**
     * @brief Compute spectral rolloff frequency (point below which a given energy percentage lies).
     *
     * @param[in] mag       Magnitude spectrum (length = N/2 + 1).
     * @param[in] freqs     Frequency bin centers (Hz, length = N/2 + 1).
     * @param[in] n_bins    Number of bins.
     * @param[in] threshold Energy fraction (0–1), typically 0.85.
     * @return Rolloff frequency in Hz.
     */
    inline float SpectralRolloff(const float *mag, const float *freqs, size_t n_bins, float threshold = 0.85f)
    {
        float total = 0.0f;
        for (size_t k = 0; k < n_bins; ++k)
            total += mag[k];

        float cumulative = 0.0f;
        for (size_t k = 0; k < n_bins; ++k)
        {
            cumulative += mag[k];
            if (cumulative >= threshold * total)
                return freqs[k];
        }
        return freqs[n_bins - 1];
    }

    /**
     * @brief Compute spectral flatness (geometric mean / arithmetic mean of magnitudes).
     *
     * @param[in] mag    Magnitude spectrum (length = N/2 + 1).
     * @param[in] n_bins Number of bins.
     * @return Flatness ratio (0 = tonal, 1 = noise-like).
     */
    inline float SpectralFlatness(const float *mag, size_t n_bins)
    {
        float geo_sum = 0.0f;
        float arith_sum = 0.0f;
        for (size_t k = 0; k < n_bins; ++k)
        {
            float val = mag[k] + 1e-12f;
            geo_sum += logf(val);
            arith_sum += val;
        }
        float geo_mean = expf(geo_sum / n_bins);
        float arith_mean = arith_sum / n_bins;
        return (arith_mean > 0.0f) ? geo_mean / arith_mean : 0.0f;
    }

    /**
     * @brief Compute spectral crest factor (ratio of max to mean magnitude).
     *
     * @param[in] mag    Magnitude spectrum (length = N/2 + 1).
     * @param[in] n_bins Number of bins.
     * @return Crest factor (higher = peaky / harmonic spectrum).
     */
    inline float SpectralCrest(const float *mag, size_t n_bins)
    {
        float max_val = 0.0f, mean = 0.0f;
        for (size_t k = 0; k < n_bins; ++k)
        {
            mean += mag[k];
            if (mag[k] > max_val)
                max_val = mag[k];
        }
        mean /= static_cast<float>(n_bins);
        return (mean > 0.0f) ? max_val / mean : 0.0f;
    }

    /**
     * @brief Compute spectral entropy (normalized energy distribution).
     *
     * @param[in] mag    Magnitude spectrum (length = N/2 + 1).
     * @param[in] n_bins Number of bins.
     * @return Entropy value in [0, 1].
     */
    inline float SpectralEntropy(const float *mag, size_t n_bins)
    {
        float sum = 0.0f;
        for (size_t k = 0; k < n_bins; ++k)
            sum += mag[k];
        if (sum <= 0.0f)
            return 0.0f;

        float entropy = 0.0f;
        for (size_t k = 0; k < n_bins; ++k)
        {
            float p = mag[k] / sum;
            if (p > 1e-9f)
                entropy -= p * log2f(p);
        }
        return entropy / log2f(static_cast<float>(n_bins));
    }

} // namespace dsp::spectral::features

/* EOF */
