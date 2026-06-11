/**
 * lora-signal-combining.cc
 *
 * Implementação das técnicas MRC / EGC / SC para sistemas SIMO LoRa.
 */

#include "lora-signal-combining.h"
#include <algorithm>
#include <numeric>
#include <stdexcept>

namespace ns3 {
namespace lora {

// ─────────────────────────────────────────────────────────────────
//  Construtor
// ─────────────────────────────────────────────────────────────────
LoraSignalCombining::LoraSignalCombining(CombiningTechnique technique,
                                         uint32_t           numAntennas)
    : m_technique(technique),
      m_numAntennas(numAntennas)
{
}

// ─────────────────────────────────────────────────────────────────
//  Combine – despacha para a técnica escolhida
// ─────────────────────────────────────────────────────────────────
CxVec
LoraSignalCombining::Combine(const CxMat& received,
                             const CxVec& channels) const
{
    if (received.size() != m_numAntennas || channels.size() != m_numAntennas)
        throw std::invalid_argument("[LoraSignalCombining] Dimensões inconsistentes.");

    switch (m_technique)
    {
        case CombiningTechnique::MRC: return MrcCombine(received, channels);
        case CombiningTechnique::EGC: return EgcCombine(received, channels);
        case CombiningTechnique::SC:  return ScCombine (received, channels);
        default:
            throw std::invalid_argument("[LoraSignalCombining] Técnica desconhecida.");
    }
}

// ─────────────────────────────────────────────────────────────────
//  MRC  –  r_combined[n] = Σ_ℓ  h*_ℓ · r_ℓ[n]  /  Σ_ℓ |h_ℓ|²
//  (equação 4 do artigo – Nguyen et al. 2021)
// ─────────────────────────────────────────────────────────────────
CxVec
LoraSignalCombining::MrcCombine(const CxMat& rs, const CxVec& hs) const
{
    uint32_t N = rs[0].size();
    CxVec    combined(N, cx_double(0.0, 0.0));

    double hPowerSum = 0.0;
    for (uint32_t l = 0; l < m_numAntennas; ++l)
        hPowerSum += std::norm(hs[l]);   // |h_ℓ|²

    if (hPowerSum < 1e-15) hPowerSum = 1e-15;   // evita divisão por zero

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

// ─────────────────────────────────────────────────────────────────
//  EGC  –  r_combined[n] = (1/L) Σ_ℓ  exp(-j·∠h_ℓ) · r_ℓ[n]
//  Remove a fase do canal mas não pondera pela amplitude.
// ─────────────────────────────────────────────────────────────────
CxVec
LoraSignalCombining::EgcCombine(const CxMat& rs, const CxVec& hs) const
{
    uint32_t N = rs[0].size();
    CxVec    combined(N, cx_double(0.0, 0.0));

    for (uint32_t l = 0; l < m_numAntennas; ++l)
    {
        double    absH = std::abs(hs[l]);
        if (absH < 1e-15) absH = 1e-15;
        cx_double phaseConj = std::conj(hs[l]) / absH;  // exp(-j·∠h_ℓ)

        for (uint32_t n = 0; n < N; ++n)
            combined[n] += phaseConj * rs[l][n];
    }

    for (uint32_t n = 0; n < N; ++n)
        combined[n] /= static_cast<double>(m_numAntennas);

    return combined;
}

// ─────────────────────────────────────────────────────────────────
//  SC  –  seleciona a antena com maior |h_ℓ|
// ─────────────────────────────────────────────────────────────────
CxVec
LoraSignalCombining::ScCombine(const CxMat& rs, const CxVec& hs) const
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
//  Demodulate  –  dechirp + FFT + argmax(|·|)
// ─────────────────────────────────────────────────────────────────
uint32_t
LoraSignalCombining::Demodulate(const CxVec& combined,
                                const CxVec& baseDownChirp,
                                uint32_t     nChips) const
{
    // Dechirp: multiplica pelo down-chirp de referência
    CxVec dechirped(nChips);
    for (uint32_t n = 0; n < nChips; ++n)
        dechirped[n] = combined[n] * baseDownChirp[n];

    // FFT
    CxVec spectrum = Fft(dechirped);

    // Argmax da magnitude
    uint32_t peakIdx = 0;
    double   peakVal = std::abs(spectrum[0]);
    for (uint32_t k = 1; k < nChips; ++k)
    {
        double v = std::abs(spectrum[k]);
        if (v > peakVal) { peakVal = v; peakIdx = k; }
    }

    return peakIdx;
}

// ─────────────────────────────────────────────────────────────────
//  FFT (DFT direta – O(N²), suficiente para SF≤12 → N≤4096)
//  Para produção recomenda-se substituir por FFTW ou KissFFT.
// ─────────────────────────────────────────────────────────────────
CxVec
LoraSignalCombining::Fft(const CxVec& x)
{
    uint32_t N = x.size();
    CxVec    X(N, cx_double(0.0, 0.0));
    const double two_pi_over_N = -2.0 * M_PI / static_cast<double>(N);

    for (uint32_t k = 0; k < N; ++k)
        for (uint32_t n = 0; n < N; ++n)
            X[k] += x[n] * std::exp(cx_double(0.0, two_pi_over_N * k * n));

    // Normalização ortogonal (equiv. ao parâmetro norm="ortho" do NumPy)
    double invSqrtN = 1.0 / std::sqrt(static_cast<double>(N));
    for (auto& v : X) v *= invSqrtN;

    return X;
}

// ─────────────────────────────────────────────────────────────────
//  ToBits  –  símbolo → SF bits (big-endian)
// ─────────────────────────────────────────────────────────────────
std::vector<int>
LoraSignalCombining::ToBits(uint32_t symbol, uint32_t sf)
{
    std::vector<int> bits(sf, 0);
    for (int i = static_cast<int>(sf) - 1; i >= 0; --i)
    {
        bits[i] = symbol & 1;
        symbol >>= 1;
    }
    return bits;
}

// ─────────────────────────────────────────────────────────────────
//  GetTechniqueName
// ─────────────────────────────────────────────────────────────────
std::string
LoraSignalCombining::GetTechniqueName() const
{
    switch (m_technique)
    {
        case CombiningTechnique::MRC: return "MRC";
        case CombiningTechnique::EGC: return "EGC";
        case CombiningTechnique::SC:  return "SC";
        default: return "Unknown";
    }
}

// ─────────────────────────────────────────────────────────────────
//  TheoreticalGainDb  –  10·log10(L)
// ─────────────────────────────────────────────────────────────────
double
LoraSignalCombining::TheoreticalGainDb(uint32_t L)
{
    return 10.0 * std::log10(static_cast<double>(L));
}

} // namespace lora
} // namespace ns3
