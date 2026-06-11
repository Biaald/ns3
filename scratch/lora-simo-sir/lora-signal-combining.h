#ifndef LORA_SIGNAL_COMBINING_H
#define LORA_SIGNAL_COMBINING_H

/**
 * lora-signal-combining.h  —  versão com suporte a SIR
 *
 * Adições vs versão SNR:
 *   - Struct EdParams: posição e SF de cada ED
 *   - Struct ChannelOutput: agora inclui potência de interferência por antena
 *   - SIR_ISOLATION_DB: matriz de isolamento inter-SF (IEEE 802.15.4g)
 *   - ApplyChannelWithInterference(): canal com K interferentes
 */

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

// ─────────────────────────────────────────────────────────────────
//  Matriz de isolamento inter-SF (dB)
//  Fonte: Croce et al., "LoRa Technology: Behind the Hardware",
//         IEEE WCNC 2020.
//  Linha = SF do ED desejado, Coluna = SF do interferente
//  SIR_ISO[i][j] = isolamento quando desejado=SF(7+i), interf=SF(7+j)
// ─────────────────────────────────────────────────────────────────
static const double SIR_ISOLATION_DB[6][6] = {
//  SF7    SF8    SF9    SF10   SF11   SF12   ← interferente
  { 6.0, -16.0, -18.0, -19.0, -19.0, -20.0 }, // desejado SF7
  {-24.0,  6.0, -20.0, -22.0, -22.0, -22.0 }, // desejado SF8
  {-27.0, -27.0,  6.0, -23.0, -25.0, -25.0 }, // desejado SF9
  {-30.0, -30.0, -30.0,  6.0, -26.0, -28.0 }, // desejado SF10
  {-33.0, -33.0, -33.0, -33.0,  6.0, -29.0 }, // desejado SF11
  {-36.0, -36.0, -36.0, -36.0, -36.0,  6.0 }, // desejado SF12
};

// ─────────────────────────────────────────────────────────────────
//  Parâmetros de um End Device
// ─────────────────────────────────────────────────────────────────
struct EdParams {
    double   x, y;      // posição (m)
    uint32_t sf;        // spreading factor (7–12)
    double   txPow_dBm; // potência de transmissão (dBm)
};

// ─────────────────────────────────────────────────────────────────
//  Saída do canal com interferência
// ─────────────────────────────────────────────────────────────────
struct ChannelOutput {
    CxMat    received;       // L × N_chips — sinal desejado + interf + ruído
    CxVec    channels;       // L coeficientes h_ℓ do ED desejado
    double   signalPow_dBm;  // potência do sinal desejado no GW
    double   interfPow_dBm;  // potência total de interferência no GW
    double   sir_dB;         // SIR = signalPow − interfPow (dB)
    double   sinr_dB;        // SINR = sinal / (interf + ruído) (dB)
};

// ─────────────────────────────────────────────────────────────────
//  Enum das técnicas
// ─────────────────────────────────────────────────────────────────
enum class CombiningTechnique { MRC, EGC, SC };

// ─────────────────────────────────────────────────────────────────
//  Classe principal
// ─────────────────────────────────────────────────────════════════
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

    // ── Utilitários de path loss ───────────────────────────────────
    // PL(dB) = 91.22 + 10·n·log10(d/d0)  com d0=1m internamente
    static double PathLossDb(double dist_m,
                             double n   = 2.0,
                             double d0  = 1.0);

    // Potência recebida: Prx(dBm) = Ptx(dBm) − PL(dB)
    static double RxPow_dBm(double txPow_dBm, double dist_m,
                             double n = 2.0);

    // Converte dBm → potência linear (mW)
    static double DbmToMw(double dBm);

    // SIR após isolamento inter-SF (dB)
    // sfDes = SF do ED desejado (7..12)
    // sfInt = SF do interferente (7..12)
    static double InterSfIsolation(uint32_t sfDes, uint32_t sfInt);

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
