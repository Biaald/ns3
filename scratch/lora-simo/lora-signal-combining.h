#ifndef LORA_SIGNAL_COMBINING_H
#define LORA_SIGNAL_COMBINING_H

/**
 * lora-signal-combining.h
 *
 * Implementação das técnicas de combinação de sinais para sistemas SIMO LoRa:
 *   - MRC  (Maximal Ratio Combining)
 *   - EGC  (Equal Gain Combining)
 *   - SC   (Selection Combining)
 *
 * Baseado em:
 *   Nguyen et al., "Performance Improvement of LoRa Modulation with
 *   Signal Combining and Semi-Coherent Detection", arXiv:2102.11509v2, 2021.
 *
 * Compatível com ns-3.40+ e o módulo lorawan (signetlab/lorawan).
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

// ─────────────────────────────────────────────────────────────────
//  Tipos auxiliares
// ─────────────────────────────────────────────────────────────────
using cx_double = std::complex<double>;
using CxVec     = std::vector<cx_double>;   // sinal no tempo (N_chips amostras)
using CxMat     = std::vector<CxVec>;       // L × N_chips  (L antenas)

// ─────────────────────────────────────────────────────────────────
//  Enum das técnicas
// ─────────────────────────────────────────────────────────────────
enum class CombiningTechnique
{
    MRC,  ///< Maximal Ratio Combining  – requer CSI  (hs conhecidos)
    EGC,  ///< Equal Gain Combining     – requer fase do canal
    SC    ///< Selection Combining      – seleciona antena de maior |h|
};

// ─────────────────────────────────────────────────────────────────
//  Classe principal
// ─────────────────────────────────────────────────────────────────
class LoraSignalCombining
{
public:
    /**
     * @param technique  Técnica de combinação (MRC, EGC ou SC).
     * @param numAntennas Número de antenas de recepção L.
     */
    LoraSignalCombining(CombiningTechnique technique, uint32_t numAntennas);

    // ── API principal ──────────────────────────────────────────────

    /**
     * Combina os sinais recebidos pelas L antenas.
     *
     * @param received  Matriz L × N_chips de sinais recebidos.
     * @param channels  Vetor de L coeficientes de canal h_ℓ (complexos).
     * @return          Sinal combinado de N_chips amostras.
     */
    CxVec Combine(const CxMat& received, const CxVec& channels) const;

    // ── Demodulação LoRa ───────────────────────────────────────────

    /**
     * Demodula o sinal combinado aplicando dechirp + FFT.
     *
     * @param combined      Sinal combinado (N_chips amostras).
     * @param baseDownChirp Down-chirp de referência (N_chips amostras).
     * @param nChips        Número de chips M = 2^SF.
     * @return              Índice do símbolo detectado.
     */
    uint32_t Demodulate(const CxVec& combined,
                        const CxVec& baseDownChirp,
                        uint32_t     nChips) const;

    // ── Utilitários ────────────────────────────────────────────────

    /** Converte número em representação de SF bits (base 2). */
    static std::vector<int> ToBits(uint32_t symbol, uint32_t sf);

    /** Retorna o nome textual da técnica. */
    std::string GetTechniqueName() const;

    /** Ganho teórico em dB: 10·log10(L). */
    static double TheoreticalGainDb(uint32_t L);

private:
    CombiningTechnique m_technique;
    uint32_t           m_numAntennas;

    // Implementações internas
    CxVec MrcCombine (const CxMat& rs, const CxVec& hs) const;
    CxVec EgcCombine (const CxMat& rs, const CxVec& hs) const;
    CxVec ScCombine  (const CxMat& rs, const CxVec& hs) const;

    // FFT simples (DFT direta – adequada para N_chips até 4096)
    static CxVec Fft(const CxVec& x);
};

} // namespace lora
} // namespace ns3

#endif // LORA_SIGNAL_COMBINING_H
