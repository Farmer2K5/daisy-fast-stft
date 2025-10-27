/**
 * @file fast_dsp_compiler.h
 * @brief Compiler-specific optimization and inlining attributes for FastDSP.
 */

#pragma once

// -----------------------------------------------------------------------------
// Compiler-specific always-inline definition
// -----------------------------------------------------------------------------
#if defined(__GNUC__) || defined(__clang__)
    #define FASTDSP_FORCE_INLINE inline __attribute__((always_inline))
#elif defined(_MSC_VER)
    #define FASTDSP_FORCE_INLINE __forceinline
#else
    #define FASTDSP_FORCE_INLINE inline
#endif

// -----------------------------------------------------------------------------
// Optional: other optimization hints (future-proofing)
// -----------------------------------------------------------------------------
#if defined(__GNUC__) || defined(__clang__)
    #define FASTDSP_UNROLL(N) _Pragma("GCC unroll N")
    #define FASTDSP_ASSUME_ALIGNED(ptr, alignment) __builtin_assume_aligned(ptr, alignment)
#else
    #define FASTDSP_UNROLL(N)
    #define FASTDSP_ASSUME_ALIGNED(ptr, alignment) (ptr)
#endif
