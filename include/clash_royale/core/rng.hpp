#pragma once

#include <cstdint>
#include <random>

namespace cr {

/// Deterministic pseudo-random source.
///
/// The simulation owns one of these and threads it through every call that
/// needs randomness, replacing the global `std::rand()` / `std::srand()` pair.
/// That matters for three reasons:
///
///   - `std::rand()` is process-global mutable state, so two simulations in one
///     process perturbed each other and nothing could be reproduced.
///   - `std::rand()`'s sequence is implementation-defined, so a seeded test
///     produced different numbers on glibc and libc++. `std::mt19937` is
///     specified by the standard, so a seed means the same thing everywhere and
///     tests can assert exact values.
///   - A seed that is recorded makes a match replayable.
class Rng {
public:
    /// Seeds the generator. The same seed always yields the same sequence,
    /// on every platform and standard library.
    explicit Rng(std::uint64_t seed = kDefaultSeed)
        : m_seed(seed), m_engine(static_cast<std::mt19937::result_type>(seed)) {}

    /// Draws a seed from the system entropy source, for when reproducibility is
    /// not wanted. The chosen seed is retrievable via seed(), so a session can
    /// still be recorded and replayed afterwards.
    static Rng fromEntropy() {
        std::random_device device;
        const auto high = static_cast<std::uint64_t>(device());
        const auto low = static_cast<std::uint64_t>(device());
        return Rng((high << 32) | low);
    }

    /// The seed this generator was constructed or reseeded with.
    std::uint64_t seed() const { return m_seed; }

    void reseed(std::uint64_t seed) {
        m_seed = seed;
        m_engine.seed(static_cast<std::mt19937::result_type>(seed));
    }

    /// Uniform integer in [0, bound). Returns 0 when bound is 0.
    ///
    /// Uses a proper uniform distribution rather than `rand() % bound`, which
    /// skews toward low values whenever bound does not divide the range.
    std::uint32_t below(std::uint32_t bound) {
        if (bound == 0) {
            return 0;
        }
        std::uniform_int_distribution<std::uint32_t> dist(0, bound - 1);
        return dist(m_engine);
    }

    /// Uniform float in [0, 1).
    float unit() {
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        return dist(m_engine);
    }

    /// True with the given probability. Probabilities <= 0 never fire and
    /// probabilities >= 1 always do, without consuming a draw, so that tests
    /// can switch a random effect fully off or fully on.
    bool chance(float probability) {
        if (probability <= 0.0f) {
            return false;
        }
        if (probability >= 1.0f) {
            return true;
        }
        return unit() < probability;
    }

    /// The underlying engine, for callers that need to drive a standard
    /// distribution directly.
    std::mt19937& engine() { return m_engine; }

private:
    static constexpr std::uint64_t kDefaultSeed = 0x9E3779B97F4A7C15ull;

    std::uint64_t m_seed;
    std::mt19937 m_engine;
};

}  // namespace cr
