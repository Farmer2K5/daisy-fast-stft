/**
 * @file fast_dsp.h
 * @brief Unified include header and documentation index for the FastDSP framework.
 *
 * @mainpage FastDSP Framework
 *
 * @section intro Introduction
 * **FastDSP** is a lightweight, modular digital signal processing framework
 * designed for real-time audio and embedded applications (such as the
 * Daisy Seed platform using the ARM CMSIS-DSP library).
 *
 * It provides a fast and consistent interface for time–frequency
 * transforms, spectral processing, and feature analysis.
 *
 * ### Core Design Goals
 * - Minimal runtime overhead
 * - Tight integration with CMSIS-DSP (`arm_math.h`)
 * - Reusable, self-contained header modules
 * - Clean and portable C++ design
 * - Ideal for 48 kHz embedded audio (e.g., ARM Cortex-M7)
 *
 * ### Architecture Overview
 * ```
 * ┌──────────────────────────┐
 * │        FastDSP           │
 * │ ┌──────────────┐         │
 * │ │   Core DSP   │         │  →  FFT, STFT, ISTFT, Windowing
 * │ └──────────────┘         │
 * │ ┌──────────────┐         │
 * │ │   Spectral   │         │  →  Conversions, Ops, Features
 * │ └──────────────┘         │
 * └──────────────────────────┘
 * ```
 *
 * @section groups Module Groups
 * - @ref FastDSPCore
 *   - @ref FastDSPCoreFFT
 *   - @ref FastDSPCoreSTFT
 *   - @ref FastDSPCoreISTFT
 *   - @ref FastDSPCoreWindow
 * - @ref FastDSPSpectral
 *   - @ref FastDSPSpectralConversions
 *   - @ref FastDSPSpectralOps
 *   - @ref FastDSPSpectralFeatures
 *
 * @section usage Usage
 * To include all FastDSP functionality:
 * @code
 * #include "fast_dsp.h"
 * @endcode
 *
 * Or include modules individually:
 * @code
 * #include "fast_stft.h"
 * #include "fast_spectral_ops.h"
 * @endcode
 *
 * @section license License
 * FastDSP is released under a permissive open license.
 * You are free to use, modify, and redistribute this code,
 * with attribution appreciated to the embedded DSP community
 * and contributors from Daisy, ARM, and Teensy forums.
 *
 * @author
 *   Developed by Robert Farmer and collaborators (2025)
 *
 * @version 1.0.0
 */

#pragma once

// -----------------------------------------------------------------------------
// Core DSP Modules
// -----------------------------------------------------------------------------
#include "fast_rfft.h"   // Real FFT wrapper and helpers
#include "fast_stft.h"   // Short-Time Fourier Transform framework
#include "fast_istft.h"  // Inverse STFT (overlap-add synthesis)
#include "fast_window.h" // Window generation and application

// -----------------------------------------------------------------------------
// Spectral-Domain Modules
// -----------------------------------------------------------------------------
#include "fast_spectral.h"          // FFT <-> Magnitude/Phase conversions
#include "fast_spectral_ops.h"      // Frequency-domain manipulation
#include "fast_spectral_features.h" // Spectral feature extraction

// -----------------------------------------------------------------------------
// Global namespace
// -----------------------------------------------------------------------------
namespace daisyfarm
{
    /**
     * @brief FastDSP library version (semantic).
     */
    static constexpr const char *version = "1.0.0";

    /**
     * @brief Global initialization banner (optional).
     * Call once to print version/build info to console/log.
     */
    inline void PrintInfo()
    {
        printf("FastDSP v%s — Lightweight DSP Framework for CMSIS-DSP\n", version);
    }
} // namespace daisyfarm

/* EOF */
