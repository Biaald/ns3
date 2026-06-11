/**
 * lora-signal-combining.cc  —  versão otimizada
 * FFT via Cooley-Tukey radix-2 in-place: O(N log N)
 */

#include "lora-signal-combining.h"
#include <algorithm>
#include <numeric>
#include <stdexcept>
#include <cstring>

namespace ns3 {
namespace lora {

LoraSignalCombining::LoraSignalCombining(CombiningTechnique technique,
                                         uint32_t           numAntennas)
    : m_technique(technique), m_numAntennas(numAntennas) {}

CxVec LoraSignalCombining::Combine(const CxMat& received,
                             const CxVec& channels) const
{
    if (received.size() != m_numAntennas || channels.size() != m_numAntennas)
        throw std::invalid_argument("[LoraSignalCombining] Dimensoes inconsistentes.");

    switch (m_technique)
    {
        case CombiningTechnique::MRC: return MrcCombine(received, channels);
        case CombiningTechnique::EGC: return EgcCombine(received, channels);
        case CombiningTechnique::SC:  return ScCombine (received, channels);
        default: throw std::invalid_argument("Tecnica desconhecida.");
    }
}

CxVec LoraSignalCombining::MrcCombine(const CxMat& rs, const CxVec& hs) const {
    uint32_t N = rs[0].size();
    CxVec combined(N, cx_double(0.0, 0.0));

    double hPowerSum = 0.0;
    for (uint32_t l = 0; l < m_numAntennas; ++l)
        hPowerSum += std::norm(hs[l]);
    if (hPowerSum < 1e-15) hPowerSum = 1e-15;

    for (uint32_t l = 0; l < m_numAntennas; ++l)
    {
        cx_double hConj = std::conj(hs[l]);
        for (uint32_t n = 0; n < N; ++n)
            combined[n] += hConj * rs[l][n];
    }
    for (uint32_t n = 0; n < N; ++n)
        combined[n] /= hPowerSum;

    return combined;
}

CxVec LoraSignalCombining::EgcCombine(const CxMat& rs, const CxVec& hs) const
{
    uint32_t N = rs[0].size();
    CxVec combined(N, cx_double(0.0, 0.0));

    for (uint32_t l = 0; l < m_numAntennas; ++l)
    {
        double absH = std::abs(hs[l]);
        if (absH < 1e-15) absH = 1e-15;
        cx_double phaseConj = std::conj(hs[l]) / absH;
        for (uint32_t n = 0; n < N; ++n)
            combined[n] += phaseConj * rs[l][n];
    }
    for (uint32_t n = 0; n < N; ++n)
        combined[n] /= static_cast<double>(m_numAntennas);

    return combined;
}

CxVec LoraSignalCombining::ScCombine(const CxMat& rs, const CxVec& hs) const
{
    uint32_t bestIdx = 0;
    double   bestPow = std::norm(hs[0]);
    for (uint32_t l = 1; l < m_numAntennas; ++l)
    {
        double p = std::norm(hs[l]);
        if (p > bestPow) { bestPow = p; bestIdx = l; }
    }
    return rs[bestIdx];
}

// ─────────────────────────────────────────────────────────────────
//  FFT Cooley-Tukey radix-2 in-place  O(N log N)
//  N deve ser potência de 2 (garantido: N = 2^SF)
// ─────────────────────────────────────────────────────────────────
CxVec LoraSignalCombining::Fft(const CxVec& x)
{
    uint32_t N = x.size();
    CxVec X(x);   // cópia de trabalho

    // ── Bit-reversal permutation ───────────────────────────────
    uint32_t bits = 0;
    while ((1u << bits) < N) ++bits;

    for (uint32_t i = 0; i < N; ++i)
    {
        uint32_t j = 0;
        for (uint32_t b = 0; b < bits; ++b)
            j |= ((i >> b) & 1u) << (bits - 1 - b);
        if (j > i) std::swap(X[i], X[j]);
    }

    // ── Butterfly stages ──────────────────────────────────────
    for (uint32_t len = 2; len <= N; len <<= 1)
    {
        double   angle = -2.0 * M_PI / static_cast<double>(len);
        cx_double wLen(std::cos(angle), std::sin(angle));

        for (uint32_t i = 0; i < N; i += len)
        {
            cx_double w(1.0, 0.0);
            for (uint32_t j = 0; j < len / 2; ++j)
            {
                cx_double u = X[i + j];
                cx_double v = X[i + j + len/2] * w;
                X[i + j]          = u + v;
                X[i + j + len/2]  = u - v;
                w *= wLen;
            }
        }
    }

    // ── Normalização ortogonal (norm="ortho") ─────────────────
    double invSqrtN = 1.0 / std::sqrt(static_cast<double>(N));
    for (auto& v : X) v *= invSqrtN;

    return X;
}

uint32_t LoraSignalCombining::Demodulate(const CxVec& combined,
                                const CxVec& baseDownChirp,
                                uint32_t     nChips) const
{
    CxVec dechirped(nChips);
    for (uint32_t n = 0; n < nChips; ++n)
        dechirped[n] = combined[n] * baseDownChirp[n];

    CxVec spectrum = Fft(dechirped);

    uint32_t peakIdx = 0;
    double   peakVal = std::norm(spectrum[0]);   // |·|² evita sqrt
    for (uint32_t k = 1; k < nChips; ++k)
    {
        double v = std::norm(spectrum[k]);
        if (v > peakVal) { peakVal = v; peakIdx = k; }
    }
    return peakIdx;
}

std::vector<int> LoraSignalCombining::ToBits(uint32_t symbol, uint32_t sf)
{
    std::vector<int> bits(sf, 0);
    for (int i = static_cast<int>(sf) - 1; i >= 0; --i)
    {
        bits[i] = symbol & 1;
        symbol >>= 1;
    }
    return bits;
}

std::string LoraSignalCombining::GetTechniqueName() const
{
    switch (m_technique)
    {
        case CombiningTechnique::MRC: return "MRC";
        case CombiningTechnique::EGC: return "EGC";
        case CombiningTechnique::SC:  return "SC";
        default: return "Unknown";
    }
}

double LoraSignalCombining::TheoreticalGainDb(uint32_t L)
{
    return 10.0 * std::log10(static_cast<double>(L));
}

} // namespace lora
} // namespace ns3
