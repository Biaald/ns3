/**
 * lora-simo-sir-simulation.cc
 *
 * Simulação ns-3.45 — SIMO LoRa com modelo de SIR
 * ─────────────────────────────────────────────────────────────────
 * Novidades vs versão SNR:
 *   - 30 EDs posicionados aleatoriamente com path loss real (distância)
 *   - Interferência co-canal: EDs com mesmo SF transmitindo no mesmo slot
 *   - Interferência inter-SF: isolamento segundo matriz Croce et al. 2020
 *   - Varredura em SIR_dB (não mais SNR nominal fixo)
 *   - Saída: BER × SIR  +  ganho em dB por técnica e L
 *
 * Parâmetros de linha de comando:
 *   --blockSize     símbolos Monte Carlo por bloco       (padrão 10000)
 *   --numBlocks     blocos máximos por ponto SIR         (padrão 3)
 *   --numEds        número de end devices                (padrão 30)
 *   --sfDes         SF do ED desejado                    (padrão 10)
 *   --lambda        taxa de chegada de pacotes (pkts/s)  (padrão 0.01)
 *   --txPow_dBm     potência de TX dos EDs (dBm)         (padrão 14)
 *
 * Rodar:
 *   ./ns3 run "scratch/lora-simo/lora-simo-sir-simulation"
 *   ./ns3 run "scratch/lora-simo/lora-simo-sir-simulation \
 *              --blockSize=10000 --numBlocks=5 --numEds=30"
 *
 * Saída:
 *   lora_sir_ber_results.csv
 *   lora_sir_gain_results.csv
 *   lora_ed_positions.csv      ← posições dos EDs para plotar topologia
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/log.h"
#include "lora-signal-combining.h"

#include <vector>
#include <complex>
#include <cmath>
#include <random>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <map>
#include <chrono>
#include <algorithm>
#include <numeric>

using namespace ns3;
using namespace ns3::lora;

NS_LOG_COMPONENT_DEFINE("LoraSimoSirSimulation");

// ═════════════════════════════════════════════════════════════════
//  Parâmetros fixos
// ═════════════════════════════════════════════════════════════════
namespace SimParams {
    constexpr double   BW_Hz      = 125e3;
    constexpr uint32_t NUM_EDS    = 30;
    constexpr double   AREA_M     = 5000.0;     // raio da célula (m)
    constexpr double   PATH_N     = 2.0;        // expoente path loss
    constexpr double   D0_M       = 1.0;        // distância de referência (m)
    constexpr double   TX_POW_DBM = 14.0;       // potência TX padrão LoRa (dBm)
    constexpr uint32_t SF_DES     = 10;         // SF do ED de interesse
    constexpr double   LAMBDA     = 0.01;       // taxa de chegada (pkts/s por ED)

    // Piso de ruído
    const double PNOISE_DBM = 10.0*std::log10(BW_Hz) - 168.0; // dBm
    const double PNOISE_MW  = std::pow(10.0, PNOISE_DBM/10.0);

    // Grade SIR (dB) — de muito ruim a muito bom
    const std::vector<double> SIR_GRID = {
        -20, -16, -12, -10, -8, -6, -4, -2,
          0,   2,   4,   6,  8, 10, 12, 16, 20
    };

    constexpr double BER_TARGET = 1e-3;
    constexpr double BER_FLOOR  = 5e-6;
    constexpr double BER_CEIL   = 0.45;

    const std::vector<uint32_t> ANT_LIST = {1, 2, 3, 4, 5};

    // SFs dos 30 EDs — distribuição típica de rede LoRaWAN
    // (EDs mais distantes usam SF maior)
    // SF7=5, SF8=5, SF9=5, SF10=5, SF11=5, SF12=5
    const std::vector<uint32_t> ED_SF_DIST = {
        7,7,7,7,7,
        8,8,8,8,8,
        9,9,9,9,9,
       10,10,10,10,10,
       11,11,11,11,11,
       12,12,12,12,12
    };
}

// ─── Timer ────────────────────────────────────────────────────────
using Clock = std::chrono::steady_clock;
static auto T0 = Clock::now();
double Elapsed() { return std::chrono::duration<double>(Clock::now()-T0).count(); }

// ─── Barra de progresso ───────────────────────────────────────────
void Progress(const std::string& lbl, uint32_t cur, uint32_t tot)
{
    constexpr uint32_t W = 25;
    uint32_t f = tot ? cur*W/tot : 0;
    double el = Elapsed();
    double eta = (cur>0 && cur<tot) ? el*(tot-cur)/cur : 0;
    std::cout << "\r  " << lbl << " [";
    for (uint32_t i=0;i<W;++i) std::cout<<(i<f?'=':' ');
    std::cout << "] " << cur << "/" << tot
              << "  " << std::fixed << std::setprecision(1)
              << el << "s ETA:" << eta << "s   " << std::flush;
    if (cur==tot) std::cout << "\n";
}

// ═════════════════════════════════════════════════════════════════
//  Down-chirp base
// ═════════════════════════════════════════════════════════════════
CxVec BuildDownChirp(uint32_t M)
{
    CxVec dc(M);
    for (uint32_t n=0; n<M; ++n) {
        double ph = -2.0*M_PI*(double)n*((double)n/(double)M);
        dc[n] = std::exp(cx_double(0,ph));
    }
    return dc;
}

// ═════════════════════════════════════════════════════════════════
//  Símbolo LoRa
// ═════════════════════════════════════════════════════════════════
CxVec BuildSymbol(uint32_t idx, uint32_t M)
{
    CxVec s(M); double pw=0;
    for (uint32_t n=0; n<M; ++n) {
        double k = (double)((idx+n)%M);
        s[n] = std::exp(cx_double(0, 2.0*M_PI*k*(double)n/(double)M));
        pw += std::norm(s[n]);
    }
    double inv = 1.0/std::sqrt(pw/(double)M);
    for (auto& v:s) v*=inv;
    return s;
}

// ═════════════════════════════════════════════════════════════════
//  Canal com interferência
//
//  Modelo:
//    r_ℓ[n] = h_ℓ · A_sig · x_m[n]          (sinal desejado)
//           + Σ_k h_ℓk · A_k · iso_k · x_mk[n]  (interferentes)
//           + w_ℓ[n]                          (ruído AWGN)
//
//  A_sig  é calculado a partir do SIR_dB e da potência de interf.
//  A_k    é a amplitude do k-ésimo interferente (path loss real).
//  iso_k  é o fator de isolamento inter-SF (linear).
// ═════════════════════════════════════════════════════════════════
ChannelOutput ApplyChannelSIR(
    const CxVec&              txSym,        // símbolo do ED desejado
    uint32_t                  sfDes,        // SF do ED desejado
    double                    sirTarget_dB, // SIR alvo que queremos impor
    const std::vector<EdParams>& interferers, // lista de interferentes ativos
    const std::vector<CxVec>& interfSyms,   // símbolos dos interferentes
    uint32_t                  numAnt,
    double                    noisePow_mW,
    std::mt19937&             rng)
{
    std::normal_distribution<double> gauss(0,1);
    uint32_t N = txSym.size();
    double   sN = std::sqrt(noisePow_mW/2.0);

    // ── Potência total de interferência (linear, mW) ──────────────
    // Usada para derivar a amplitude do sinal desejado a partir do SIR alvo
    double Pinterf_mW = 0.0;
    for (size_t k=0; k<interferers.size(); ++k) {
        double dist = std::sqrt(interferers[k].x*interferers[k].x +
                                interferers[k].y*interferers[k].y);
        double Prx_dBm = LoraSignalCombining::RxPow_dBm(
                             interferers[k].txPow_dBm, dist, SimParams::PATH_N);
        double iso_dB  = LoraSignalCombining::InterSfIsolation(
                             sfDes, interferers[k].sf);
        // Interferência efetiva = Prx − |isolamento|
        double Peff_dBm = Prx_dBm + iso_dB; // iso_dB é negativo se SF diferente
        Pinterf_mW += LoraSignalCombining::DbmToMw(Peff_dBm);
    }
    if (Pinterf_mW < 1e-15) Pinterf_mW = 1e-15;

    // Amplitude do sinal desejado que produz o SIR alvo
    double sirLin   = std::pow(10.0, sirTarget_dB/10.0);
    double Psig_mW  = sirLin * Pinterf_mW;
    double Asig     = std::sqrt(Psig_mW);

    // SINR para log
    double Pnoise_mW = noisePow_mW * N; // potência total do ruído no símbolo
    double sinr_dB   = 10.0*std::log10(Psig_mW/(Pinterf_mW+Pnoise_mW));

    ChannelOutput out;
    out.received.resize(numAnt, CxVec(N));
    out.channels.resize(numAnt);
    out.signalPow_dBm = 10.0*std::log10(Psig_mW);
    out.interfPow_dBm = 10.0*std::log10(Pinterf_mW);
    out.sir_dB        = sirTarget_dB;
    out.sinr_dB       = sinr_dB;

    for (uint32_t l=0; l<numAnt; ++l)
    {
        // Canal do ED desejado: Rayleigh CN(0,1)
        cx_double h(gauss(rng)/M_SQRT2, gauss(rng)/M_SQRT2);
        out.channels[l] = h;

        for (uint32_t n=0; n<N; ++n)
        {
            // Sinal desejado
            cx_double rx = h * Asig * txSym[n];

            // Soma dos interferentes
            for (size_t k=0; k<interferers.size(); ++k)
            {
                double dist = std::sqrt(interferers[k].x*interferers[k].x +
                                        interferers[k].y*interferers[k].y);
                double Prx_dBm = LoraSignalCombining::RxPow_dBm(
                                     interferers[k].txPow_dBm, dist,
                                     SimParams::PATH_N);
                double iso_dB  = LoraSignalCombining::InterSfIsolation(
                                     sfDes, interferers[k].sf);
                double Ak = std::sqrt(
                    LoraSignalCombining::DbmToMw(Prx_dBm + iso_dB));

                // Canal independente por interferente e antena
                cx_double hk(gauss(rng)/M_SQRT2, gauss(rng)/M_SQRT2);
                rx += hk * Ak * interfSyms[k][n];
            }

            // Ruído AWGN
            rx += cx_double(gauss(rng)*sN, gauss(rng)*sN);
            out.received[l][n] = rx;
        }
    }
    return out;
}

// ═════════════════════════════════════════════════════════════════
//  Sorteia quais EDs estão ativos (colisão por Poisson)
//  Prob. de ativo num slot = 1 − exp(−λ·Tsym)
// ═════════════════════════════════════════════════════════════════
std::vector<bool> ActiveEds(uint32_t numEds, double lambda,
                             uint32_t sfDes, uint32_t M,
                             double BW_Hz, std::mt19937& rng)
{
    double Tsym = (double)M / BW_Hz;         // duração do símbolo (s)
    double pActive = 1.0 - std::exp(-lambda * Tsym);
    std::bernoulli_distribution bern(pActive);
    std::vector<bool> active(numEds);
    for (uint32_t i=0; i<numEds; ++i) active[i] = bern(rng);
    return active;
}

// ═════════════════════════════════════════════════════════════════
//  Um bloco Monte Carlo com SIR
// ═════════════════════════════════════════════════════════════════
uint64_t SimulateBlockSIR(
    CombiningTechnique           tech,
    uint32_t                     numAnt,
    double                       sirTarget_dB,
    uint32_t                     blockSize,
    const CxVec&                 dc,
    const std::vector<EdParams>& allEds,   // todos os 30 EDs
    uint32_t                     desiredIdx,// índice do ED de interesse
    double                       lambda,
    std::mt19937&                rng)
{
    const uint32_t SF = SimParams::SF_DES;
    const uint32_t M  = 1u << SF;

    LoraSignalCombining combiner(tech, numAnt);
    std::uniform_int_distribution<uint32_t> symDist(0, M-1);
    uint64_t errors = 0;

    for (uint32_t t=0; t<blockSize; ++t)
    {
        // Símbolo do ED desejado
        uint32_t txSym = symDist(rng);
        CxVec    sym   = BuildSymbol(txSym, M);

        // Determina quais outros EDs estão transmitindo neste slot
        auto active = ActiveEds((uint32_t)allEds.size(), lambda,
                                SF, M, SimParams::BW_Hz, rng);

        // Monta lista de interferentes (exclui o ED desejado)
        std::vector<EdParams> interfList;
        std::vector<CxVec>   interfSyms;
        for (uint32_t i=0; i<(uint32_t)allEds.size(); ++i) {
            if (i == desiredIdx) continue;
            if (!active[i]) continue;
            interfList.push_back(allEds[i]);
            interfSyms.push_back(BuildSymbol(symDist(rng), M));
        }

        // Canal com interferência
        auto ch = ApplyChannelSIR(sym, SF, sirTarget_dB,
                                   interfList, interfSyms,
                                   numAnt, SimParams::PNOISE_MW, rng);

        // Combining + demodulação
        auto comb = combiner.Combine(ch.received, ch.channels);
        uint32_t rxSym = combiner.Demodulate(comb, dc, M);

        // Conta erros de bit
        auto txB = LoraSignalCombining::ToBits(txSym, SF);
        auto rxB = LoraSignalCombining::ToBits(rxSym, SF);
        for (uint32_t b=0; b<SF; ++b)
            errors += (txB[b]!=rxB[b]) ? 1 : 0;
    }
    return errors;
}

// ═════════════════════════════════════════════════════════════════
//  BER para um ponto SIR
// ═════════════════════════════════════════════════════════════════
double SimulateBerSIR(
    CombiningTechnique           tech,
    uint32_t                     numAnt,
    double                       sirTarget_dB,
    uint32_t                     blockSize,
    uint32_t                     numBlocks,
    const CxVec&                 dc,
    const std::vector<EdParams>& allEds,
    uint32_t                     desiredIdx,
    double                       lambda,
    std::mt19937&                rng)
{
    const uint32_t SF = SimParams::SF_DES;
    uint64_t totErr=0, totBits=0;

    for (uint32_t b=0; b<numBlocks; ++b) {
        totErr  += SimulateBlockSIR(tech, numAnt, sirTarget_dB,
                                     blockSize, dc, allEds,
                                     desiredIdx, lambda, rng);
        totBits += (uint64_t)blockSize * SF;

        double ber = totBits>0 ? (double)totErr/totBits : 1.0;
        if (b>=1 && ber < SimParams::BER_FLOOR) break;
        if (b>=1 && ber > SimParams::BER_CEIL)  break;
    }
    return totBits>0 ? (double)totErr/totBits : 1.0;
}

// ═════════════════════════════════════════════════════════════════
//  Interpolação SIR @ BER_TARGET
// ═════════════════════════════════════════════════════════════════
double SirAtTarget(const std::vector<double>& ber,
                   const std::vector<double>& sirGrid)
{
    for (size_t i=1; i<ber.size(); ++i) {
        if (ber[i]<=SimParams::BER_TARGET && ber[i-1]>SimParams::BER_TARGET) {
            double lb0 = std::log10(std::max(ber[i-1],1e-10));
            double lb1 = std::log10(std::max(ber[i],  1e-10));
            double lt  = std::log10(SimParams::BER_TARGET);
            double f   = (lt-lb0)/(lb1-lb0);
            return sirGrid[i-1] + f*(sirGrid[i]-sirGrid[i-1]);
        }
    }
    return sirGrid.back();
}

// ═════════════════════════════════════════════════════════════════
//  main
// ═════════════════════════════════════════════════════════════════
int main(int argc, char* argv[])
{
    uint32_t blockSize  = 10000;
    uint32_t numBlocks  = 3;
    uint32_t numEds     = SimParams::NUM_EDS;
    uint32_t sfDes      = SimParams::SF_DES;
    double   lambda     = SimParams::LAMBDA;
    double   txPow_dBm  = SimParams::TX_POW_DBM;

    CommandLine cmd(__FILE__);
    cmd.AddValue("blockSize",  "Simbolos por bloco",          blockSize);
    cmd.AddValue("numBlocks",  "Blocos maximos por ponto SIR", numBlocks);
    cmd.AddValue("numEds",     "Numero de end devices",        numEds);
    cmd.AddValue("sfDes",      "SF do ED desejado (7-12)",     sfDes);
    cmd.AddValue("lambda",     "Taxa de chegada pkts/s por ED",lambda);
    cmd.AddValue("txPow_dBm",  "Potencia TX dos EDs (dBm)",    txPow_dBm);
    cmd.Parse(argc, argv);

    LogComponentEnable("LoraSimoSirSimulation", LOG_LEVEL_INFO);

    NS_LOG_INFO("=== Simulacao SIMO LoRa com SIR — ns-3.45 ===");
    NS_LOG_INFO("numEds="    << numEds
                << "  sfDes=" << sfDes
                << "  lambda=" << lambda << " pkts/s"
                << "  txPow="  << txPow_dBm << " dBm"
                << "  blockSize=" << blockSize
                << "  numBlocks=" << numBlocks);

    std::mt19937 rng(42);

    // ── Down-chirp ────────────────────────────────────────────────
    uint32_t M  = 1u << sfDes;
    CxVec    dc = BuildDownChirp(M);

    // ── Topologia ns-3: posiciona os EDs ─────────────────────────
    NodeContainer edNodes, gwNode;
    edNodes.Create(numEds);
    gwNode.Create(1);

    MobilityHelper mob;
    mob.SetPositionAllocator("ns3::UniformDiscPositionAllocator",
        "X",   DoubleValue(0.0),
        "Y",   DoubleValue(0.0),
        "rho", DoubleValue(SimParams::AREA_M));
    mob.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mob.Install(edNodes);

    MobilityHelper gwMob;
    gwMob.SetPositionAllocator("ns3::GridPositionAllocator",
        "MinX", DoubleValue(0.0), "MinY", DoubleValue(0.0));
    gwMob.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    gwMob.Install(gwNode);

    // Extrai posições reais e monta EdParams
    std::vector<EdParams> allEds(numEds);
    std::ofstream posFile("lora_ed_positions.csv");
    posFile << "ed_id,x_m,y_m,dist_m,sf,txPow_dBm,rxPow_dBm\n";

    for (uint32_t i=0; i<numEds; ++i) {
        Ptr<MobilityModel> mm = edNodes.Get(i)->GetObject<MobilityModel>();
        Vector pos = mm->GetPosition();
        allEds[i].x         = pos.x;
        allEds[i].y         = pos.y;
        allEds[i].sf        = SimParams::ED_SF_DIST[i % SimParams::ED_SF_DIST.size()];
        allEds[i].txPow_dBm = txPow_dBm;

        double dist   = std::sqrt(pos.x*pos.x + pos.y*pos.y);
        double rxPow  = LoraSignalCombining::RxPow_dBm(txPow_dBm, dist,
                                                         SimParams::PATH_N);
        posFile << i << "," << pos.x << "," << pos.y << ","
                << dist << "," << allEds[i].sf << ","
                << txPow_dBm << "," << rxPow << "\n";
    }
    posFile.close();
    NS_LOG_INFO("Posicoes salvas: lora_ed_positions.csv");

    // Escolhe o ED de interesse: o de SF=sfDes mais próximo do centro
    uint32_t desiredIdx = 0;
    double   bestDist   = 1e18;
    for (uint32_t i=0; i<numEds; ++i) {
        if (allEds[i].sf != sfDes) continue;
        double d = std::sqrt(allEds[i].x*allEds[i].x +
                             allEds[i].y*allEds[i].y);
        if (d < bestDist) { bestDist = d; desiredIdx = i; }
    }
    NS_LOG_INFO("ED desejado: idx=" << desiredIdx
                << "  SF=" << allEds[desiredIdx].sf
                << "  dist=" << std::fixed << std::setprecision(1)
                << bestDist << " m");

    // ── Técnicas ──────────────────────────────────────────────────
    std::vector<std::pair<CombiningTechnique,std::string>> techs = {
        {CombiningTechnique::SC,  "SC"},
        {CombiningTechnique::EGC, "EGC"},
        {CombiningTechnique::MRC, "MRC"},
    };

    const auto& sirGrid = SimParams::SIR_GRID;
    uint32_t nSir     = sirGrid.size();
    uint32_t grandTot = techs.size() * SimParams::ANT_LIST.size() * nSir;
    uint32_t done     = 0;

    std::ofstream berFile("lora_sir_ber_results.csv");
    berFile << "technique,L,SIR_dB,BER\n";
    berFile << std::fixed << std::setprecision(8);

    std::map<std::string,std::map<uint32_t,std::vector<double>>> allBer;

    // ── Loop principal ────────────────────────────────────────────
    for (auto& [tech, tName] : techs)
    {
        NS_LOG_INFO("\n--- Tecnica: " << tName << " ---");

        for (uint32_t L : SimParams::ANT_LIST)
        {
            auto tStart = Clock::now();
            std::cout << "  " << tName << " L=" << L << "\n";

            for (uint32_t si=0; si<nSir; ++si)
            {
                double sir = sirGrid[si];

                double ber = SimulateBerSIR(
                    tech, L, sir,
                    blockSize, numBlocks, dc,
                    allEds, desiredIdx, lambda, rng);

                berFile << tName << "," << L << ","
                        << sir << "," << ber << "\n";
                berFile.flush();
                allBer[tName][L].push_back(ber);
                ++done;

                double el = std::chrono::duration<double>(
                    Clock::now()-tStart).count();
                Progress(tName+" L="+std::to_string(L), si+1, nSir);
            }

            double elapsed = std::chrono::duration<double>(
                Clock::now()-tStart).count();
            NS_LOG_INFO("  " << tName << " L=" << L
                        << "  tempo=" << std::fixed
                        << std::setprecision(1) << elapsed << "s"
                        << "  (" << done << "/" << grandTot << ")");
        }
    }
    berFile.close();
    NS_LOG_INFO("\nBER salvo: lora_sir_ber_results.csv");

    // ── Análise de ganho (SIR) ────────────────────────────────────
    std::ofstream gainFile("lora_sir_gain_results.csv");
    gainFile << "technique,L,SIR_at_BER1e-3_dB,gain_sim_dB,gain_theo_10logN_dB\n";
    gainFile << std::fixed << std::setprecision(4);

    NS_LOG_INFO("\n=== Ganho de Diversidade — metrica SIR (BER=1e-3) ===");
    NS_LOG_INFO(std::left
                << std::setw(6)  << "Tech"
                << std::setw(4)  << "L"
                << std::setw(14) << "SIR@BER(dB)"
                << std::setw(14) << "Ganho(dB)"
                << "10log10(L)");

    for (auto& [tech,tName] : techs)
    {
        double sirL1 = SirAtTarget(allBer[tName][1], sirGrid);
        for (uint32_t L : SimParams::ANT_LIST)
        {
            double sirL  = SirAtTarget(allBer[tName][L], sirGrid);
            double gSim  = sirL1 - sirL;
            double gTheo = LoraSignalCombining::TheoreticalGainDb(L);

            gainFile << tName << "," << L << ","
                     << sirL  << "," << gSim << "," << gTheo << "\n";

            NS_LOG_INFO(std::left << std::fixed << std::setprecision(2)
                        << std::setw(6)  << tName
                        << std::setw(4)  << L
                        << std::setw(14) << sirL
                        << std::setw(14) << gSim
                        << gTheo);
        }
    }
    gainFile.close();
    NS_LOG_INFO("Ganho salvo: lora_sir_gain_results.csv");

    NS_LOG_INFO("\nTempo total: " << std::fixed << std::setprecision(1)
                << Elapsed() << "s");
    NS_LOG_INFO("Simulacao concluida.");
    return 0;
}
