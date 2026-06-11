/**
 * lora-signal-combining.cc  —  versão com SIR
 * FFT Cooley-Tukey O(N log N) + utilitários de path loss e isolamento inter-SF
 */

#include "lora-signal-combining.h"
#include <algorithm>
#include <stdexcept>

namespace ns3 {
namespace lora {

LoraSignalCombining::LoraSignalCombining(CombiningTechnique t, uint32_t L)
    : m_technique(t), m_numAntennas(L) {}

// ─────────────────────────────────────────────────────────────────
//  Combine
// ─────────────────────────────────────────────────────────────────
CxVec LoraSignalCombining::Combine(const CxMat& rs, const CxVec& hs) const
{
    if (rs.size() != m_numAntennas || hs.size() != m_numAntennas)
        throw std::invalid_argument("Dimensoes inconsistentes.");
    switch (m_technique) {
        case CombiningTechnique::MRC: return MrcCombine(rs, hs);
        case CombiningTechnique::EGC: return EgcCombine(rs, hs);
        case CombiningTechnique::SC:  return ScCombine (rs, hs);
        default: throw std::invalid_argument("Tecnica desconhecida.");
    }
}

CxVec LoraSignalCombining::MrcCombine(const CxMat& rs, const CxVec& hs) const
{
    uint32_t N = rs[0].size();
    CxVec combined(N, cx_double(0,0));
    double hpow = 0;
    for (uint32_t l = 0; l < m_numAntennas; ++l) hpow += std::norm(hs[l]);
    if (hpow < 1e-15) hpow = 1e-15;
    for (uint32_t l = 0; l < m_numAntennas; ++l) {
        cx_double hc = std::conj(hs[l]);
        for (uint32_t n = 0; n < N; ++n) combined[n] += hc * rs[l][n];
    }
    for (auto& v : combined) v /= hpow;
    return combined;
}

CxVec LoraSignalCombining::EgcCombine(const CxMat& rs, const CxVec& hs) const
{
    uint32_t N = rs[0].size();
    CxVec combined(N, cx_double(0,0));
    for (uint32_t l = 0; l < m_numAntennas; ++l) {
        double a = std::abs(hs[l]); if (a < 1e-15) a = 1e-15;
        cx_double pc = std::conj(hs[l]) / a;
        for (uint32_t n = 0; n < N; ++n) combined[n] += pc * rs[l][n];
    }
    for (auto& v : combined) v /= static_cast<double>(m_numAntennas);
    return combined;
}

CxVec LoraSignalCombining::ScCombine(const CxMat& rs, const CxVec& hs) const
{
    uint32_t best = 0; double bp = std::norm(hs[0]);
    for (uint32_t l = 1; l < m_numAntennas; ++l) {
        double p = std::norm(hs[l]); if (p > bp) { bp = p; best = l; }
    }
    return rs[best];
}

// ─────────────────────────────────────────────────────────────────
//  FFT Cooley-Tukey radix-2 in-place  O(N log N)
// ─────────────────────────────────────────────────────────────────
CxVec LoraSignalCombining::Fft(const CxVec& x)
{
    uint32_t N = x.size();
    CxVec X(x);
    uint32_t bits = 0;
    while ((1u << bits) < N) ++bits;
    for (uint32_t i = 0; i < N; ++i) {
        uint32_t j = 0;
        for (uint32_t b = 0; b < bits; ++b) j |= ((i>>b)&1u)<<(bits-1-b);
        if (j > i) std::swap(X[i], X[j]);
    }
    for (uint32_t len = 2; len <= N; len <<= 1) {
        double ang = -2.0*M_PI/len;
        cx_double wl(std::cos(ang), std::sin(ang));
        for (uint32_t i = 0; i < N; i += len) {
            cx_double w(1,0);
            for (uint32_t j = 0; j < len/2; ++j) {
                cx_double u = X[i+j], v = X[i+j+len/2]*w;
                X[i+j] = u+v; X[i+j+len/2] = u-v; w *= wl;
            }
        }
    }
    double inv = 1.0/std::sqrt((double)N);
    for (auto& v : X) v *= inv;
    return X;
}

uint32_t LoraSignalCombining::Demodulate(const CxVec& combined,
                                          const CxVec& dc, uint32_t N) const
{
    CxVec dechirped(N);
    for (uint32_t n = 0; n < N; ++n) dechirped[n] = combined[n] * dc[n];
    CxVec sp = Fft(dechirped);
    uint32_t peak = 0; double pv = std::norm(sp[0]);
    for (uint32_t k = 1; k < N; ++k) {
        double v = std::norm(sp[k]); if (v > pv) { pv = v; peak = k; }
    }
    return peak;
}

std::vector<int> LoraSignalCombining::ToBits(uint32_t sym, uint32_t sf)
{
    std::vector<int> bits(sf, 0);
    for (int i = (int)sf-1; i >= 0; --i) { bits[i] = sym&1; sym >>= 1; }
    return bits;
}

std::string LoraSignalCombining::GetTechniqueName() const
{
    switch (m_technique) {
        case CombiningTechnique::MRC: return "MRC";
        case CombiningTechnique::EGC: return "EGC";
        case CombiningTechnique::SC:  return "SC";
        default: return "Unknown";
    }
}

double LoraSignalCombining::TheoreticalGainDb(uint32_t L)
{ return 10.0*std::log10((double)L); }

// ─────────────────────────────────────────────────────────────────
//  Utilitários de path loss e isolamento inter-SF
// ─────────────────────────────────────────────────────────────────
double LoraSignalCombining::PathLossDb(double dist_m, double n, double d0)
{
    if (dist_m < 1.0) dist_m = 1.0;   // evita log(0)
    // Adaptado de: 91.22 + 10n·log10(d/d0)  com d0 em metros
    return 91.22 + 10.0*n*std::log10(dist_m/d0);
}

double LoraSignalCombining::RxPow_dBm(double txPow_dBm, double dist_m, double n)
{ return txPow_dBm - PathLossDb(dist_m, n); }

double LoraSignalCombining::DbmToMw(double dBm)
{ return std::pow(10.0, dBm/10.0); }

double LoraSignalCombining::InterSfIsolation(uint32_t sfDes, uint32_t sfInt)
{
    uint32_t i = sfDes - 7;
    uint32_t j = sfInt - 7;
    if (i > 5) i = 5;
    if (j > 5) j = 5;
    return SIR_ISOLATION_DB[i][j];
}

} // namespace lora
} // namespace ns3
