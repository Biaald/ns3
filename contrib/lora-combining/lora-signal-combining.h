#ifndef LORA_SIGNAL_COMBINING_H
#define LORA_SIGNAL_COMBINING_H

#include "ns3/nstime.h"
#include "ns3/type-id.h"
#include <vector>
#include <complex>
#include <cmath>
#include <stdexcept>
#include <string>

namespace ns3 {
namespace lora {

using cx_double = std::complex<double>;
using CxVec     = std::vector<cx_double>;
using CxMat     = std::vector<CxVec>;

enum class CombiningTechnique { MRC, EGC, SC };

class LoraSignalCombining
{
public:
    LoraSignalCombining(CombiningTechnique technique, uint32_t numAntennas);

    CxVec    Combine   (const CxMat& received, const CxVec& channels) const;
    uint32_t Demodulate(const CxVec& combined,
                        const CxVec& baseDownChirp,
                        uint32_t     nChips) const;

    static std::vector<int> ToBits(uint32_t symbol, uint32_t sf);
    std::string             GetTechniqueName() const;
    static double           TheoreticalGainDb(uint32_t L);

private:
    CombiningTechnique m_technique;
    uint32_t           m_numAntennas;

    CxVec MrcCombine(const CxMat& rs, const CxVec& hs) const;
    CxVec EgcCombine(const CxMat& rs, const CxVec& hs) const;
    CxVec ScCombine (const CxMat& rs, const CxVec& hs) const;

    static CxVec Fft(const CxVec& x);
};

} // namespace lora
} // namespace ns3

#endif // LORA_SIGNAL_COMBINING_H
