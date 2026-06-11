/**
 * lora-network-test.cc 
 *
 * Teste completo da rede LoRa com combining de antenas
 * ─────────────────────────────────────────────────────────────────
 * Estrutura de rede idêntica ao complete-network-example.cc
 * Adicionado: configuração de SC/EGC/MRC no LoraInterferenceHelper
 *
 * Rodar:
 *   ./ns3 run "scratch/lora-simo/lora-network-test"
 *   ./ns3 run "scratch/lora-simo/lora-network-test \
 *              --nDevices=30 --radius=5000 --simTime=3600 \
 *              --technique=MRC --nAntennas=4 --realisticChannelModel=true"
 * 
 * ./ns3 run "scratch/lora-simo/lora-network-test \
    --nDevices=500 \
    --radius=2000 \
    --simTime=3600 \
    --appPeriod=10 \
    --technique=ALL \
    --nAntennas=0 \
    --realisticChannel=true"
 */

#include "ns3/class-a-end-device-lorawan-mac.h"
#include "ns3/command-line.h"
#include "ns3/constant-position-mobility-model.h"
#include "ns3/double.h"
#include "ns3/end-device-lora-phy.h"
#include "ns3/forwarder-helper.h"
#include "ns3/gateway-lora-phy.h"
#include "ns3/gateway-lorawan-mac.h"
#include "ns3/log.h"
#include "ns3/lora-helper.h"
#include "ns3/lora-interference-helper.h"
#include "ns3/lora-packet-tracker.h"
#include "ns3/mobility-helper.h"
#include "ns3/network-server-helper.h"
#include "ns3/node-container.h"
#include "ns3/periodic-sender-helper.h"
#include "ns3/pointer.h"
#include "ns3/position-allocator.h"
#include "ns3/random-variable-stream.h"
#include "ns3/simulator.h"
#include "ns3/point-to-point-module.h"

// canal mais realista
#include "ns3/building-allocator.h"
#include "ns3/building-penetration-loss.h"
#include "ns3/buildings-helper.h"
#include "ns3/correlated-shadowing-propagation-loss-model.h"

// combining 
#include "ns3/lora-interference-helper.h"

#include <fstream>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <cmath>

using namespace ns3;
using namespace ns3::lorawan;

NS_LOG_COMPONENT_DEFINE("LoraNetworkTest");

// Timer 
using Clock = std::chrono::steady_clock;
static auto T0 = Clock::now();
double Elapsed() {
    return std::chrono::duration<double>(Clock::now() - T0).count();
}

//  Resultado de uma rodada
struct SimResult {

    std::string technique;
    uint32_t L;

    uint32_t sent;
    uint32_t received;
    uint32_t interfered;
    uint32_t noMoreGw;
    uint32_t underSensitivity;
    uint32_t lostTx;

    double pdr;
    double per;

    double interferenceLossRate;
    double sensitivityLossRate;
    double gatewayBusyRate;
    double txLossRate;

    double elapsedSec;

};

//  Tabela de ganhos do Python (SNR @ BER=10⁻³, Rayleigh, SF=10)
double GetPythonGainDb(const std::string& tech, uint32_t L)
{
    static const double gSC[5]  = {0.00, 12.58, 16.80, 19.11, 20.45};
    static const double gEGC[5] = {0.00, 13.47, 18.38, 21.44, 23.08};
    static const double gMRC[5] = {0.00, 14.16, 19.38, 22.38, 24.28};
    uint32_t idx = std::min(L, uint32_t(5)) - 1;
    if (tech == "SC")  return gSC[idx];
    if (tech == "EGC") return gEGC[idx];
    if (tech == "MRC") return gMRC[idx];
    return 0.0;
}

//  Configura combining no LoraInterferenceHelper do gateway
void ConfigureCombining(NodeContainer&     gateways,
                        const std::string& techniqueName,
                        uint32_t           numAntennas)
{
    for (uint32_t i = 0; i < gateways.GetN(); ++i)
    {
        Ptr<Node>           gwNode    = gateways.Get(i);
        Ptr<LoraNetDevice>  loraDev   =
            DynamicCast<LoraNetDevice>(gwNode->GetDevice(0));

        if (!loraDev) {
            NS_LOG_WARN("ConfigureCombining: LoraNetDevice nao encontrado");
            continue;
        }

        Ptr<GatewayLoraPhy> gwPhy =
            DynamicCast<GatewayLoraPhy>(loraDev->GetPhy());

        if (!gwPhy) {
            NS_LOG_WARN("ConfigureCombining: GatewayLoraPhy nao encontrado");
            continue;
        }

        LoraInterferenceHelper* helper = gwPhy->GetInterferenceHelper();

        if (!helper) {
            NS_LOG_WARN("ConfigureCombining: LoraInterferenceHelper nao encontrado");
            continue;
        }

        helper->SetCollisionMatrix(LoraInterferenceHelper::GOURSAUD);
        helper->SetNumAntennas(numAntennas);

        if      (techniqueName == "SC")
            helper->SetCombiningMethod(LoraInterferenceHelper::SC);
        else if (techniqueName == "EGC")
            helper->SetCombiningMethod(LoraInterferenceHelper::EGC);
        else if (techniqueName == "MRC")
            helper->SetCombiningMethod(LoraInterferenceHelper::MRC);
        else
            helper->SetCombiningMethod(LoraInterferenceHelper::SINGLE_ANTENNA);

        NS_LOG_INFO("GW[" << i << "] combining=" << techniqueName
                    << " L=" << numAntennas);
    }
}

//  Roda uma simulação 
SimResult RunSimulation(
    int         nDevices,
    double      radiusMeters,
    double      simulationTimeSeconds,
    int         appPeriodSeconds,
    std::string techniqueName,
    uint32_t    numAntennas,
    uint32_t    seed,
    bool        realisticChannelModel,
    const std::vector<Vector>& fixedPositions,
    const std::vector<double>& fixedStartTimes)
{
    RngSeedManager::SetSeed(seed);
    RngSeedManager::SetRun(1);

    auto tStart = Clock::now();

    // Mobilidade 
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc =
    CreateObject<ListPositionAllocator>();

    for (const auto& pos : fixedPositions)
    {
        positionAlloc->Add(pos);
    }

    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    /*mobility.SetPositionAllocator(
        "ns3::UniformDiscPositionAllocator",
        "rho", DoubleValue(radiusMeters),
        "X",   DoubleValue(0.0),
        "Y",   DoubleValue(0.0));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");*/

    // Canal 
    Ptr<LogDistancePropagationLossModel> loss = CreateObject<LogDistancePropagationLossModel>();
    loss->SetPathLossExponent(3.76);
    loss->SetReference(1, 7.7); // 7.7 dB at 1 m (LoRa SF10, 868 MHz, free space)

    if (realisticChannelModel)
    {
        // Create the correlated shadowing component
        Ptr<CorrelatedShadowingPropagationLossModel> shadowing =
            CreateObject<CorrelatedShadowingPropagationLossModel>();

        // Aggregate shadowing to the logdistance loss
        loss->SetNext(shadowing);

        // Add the effect to the channel propagation loss
        Ptr<BuildingPenetrationLoss> buildingLoss = CreateObject<BuildingPenetrationLoss>();

        shadowing->SetNext(buildingLoss);
    }

    Ptr<PropagationDelayModel> delay =
        CreateObject<ConstantSpeedPropagationDelayModel>();

    Ptr<LoraChannel> channel = CreateObject<LoraChannel>(loss, delay);

    // Helpers
    LoraPhyHelper    phyHelper;
    LorawanMacHelper macHelper;
    LoraHelper       loraHelper;

    phyHelper.SetChannel(channel);
    loraHelper.EnablePacketTracking();

    NetworkServerHelper nsHelper;
    ForwarderHelper     forHelper;

    // End Devices 
    NodeContainer endDevices;
    endDevices.Create(nDevices);
    mobility.Install(endDevices);

    // Altura dos EDs = 1.2 m 
    for (auto j = endDevices.Begin(); j != endDevices.End(); ++j)
    {
        Ptr<MobilityModel> mob = (*j)->GetObject<MobilityModel>();
        Vector pos = mob->GetPosition();
        pos.z = 1.2;
        mob->SetPosition(pos);
    }

    // Gerador de endereços LoRa
    uint8_t  nwkId   = 54;
    uint32_t nwkAddr = 1864;
    Ptr<LoraDeviceAddressGenerator> addrGen =
        CreateObject<LoraDeviceAddressGenerator>(nwkId, nwkAddr);
    macHelper.SetAddressGenerator(addrGen);

    phyHelper.SetDeviceType(LoraPhyHelper::ED);
    macHelper.SetDeviceType(LorawanMacHelper::ED_A);
    loraHelper.Install(phyHelper, macHelper, endDevices);

    // Gateway 
    NodeContainer gateways;
    gateways.Create(1);

    Ptr<ListPositionAllocator> gwPos =
        CreateObject<ListPositionAllocator>();
    gwPos->Add(Vector(0.0, 0.0, 15.0));
    mobility.SetPositionAllocator(gwPos);
    mobility.Install(gateways);

    phyHelper.SetDeviceType(LoraPhyHelper::GW);
    macHelper.SetDeviceType(LorawanMacHelper::GW);
    loraHelper.Install(phyHelper, macHelper, gateways);

    if (realisticChannelModel)
    {
        BuildingsHelper::Install(endDevices);
        BuildingsHelper::Install(gateways);
    }

    // Configura combining após instalar PHY do GW
    ConfigureCombining(gateways, techniqueName, numAntennas);

    // SF automático por distância
    LorawanMacHelper::SetSpreadingFactorsUp(endDevices, gateways, channel);
    std::cout << "\nDistribuição de SF\n";

    std::map<int,int> sfCount;

    for (auto it = endDevices.Begin(); it != endDevices.End(); ++it)
    {
        Ptr<LoraNetDevice> dev =
            DynamicCast<LoraNetDevice>((*it)->GetDevice(0));

        Ptr<ClassAEndDeviceLorawanMac> mac =
            DynamicCast<ClassAEndDeviceLorawanMac>(
                dev->GetMac());

        sfCount[mac->GetDataRate()]++;
    }

    for (auto const& p : sfCount)
    {
        std::cout
            << "DR" << p.first
            << " = "
            << p.second
            << std::endl;
    }

    // Tráfego periódico
    Time appStopTime = Seconds(simulationTimeSeconds);

    PeriodicSenderHelper appHelper;
    appHelper.SetPeriod(Seconds(appPeriodSeconds));
    appHelper.SetPacketSize(23);   // mesmo tamanho do exemplo oficial

    ApplicationContainer appContainer = appHelper.Install(endDevices);

    //appContainer.Start(Seconds(0));
    /*Ptr<UniformRandomVariable> startRv = CreateObject<UniformRandomVariable>();

    startRv->SetAttribute("Min", DoubleValue(0));
    startRv->SetAttribute("Max", DoubleValue(appPeriodSeconds));

    for (uint32_t i = 0;
        i < appContainer.GetN();
        ++i)
    {
        appContainer.Get(i)->SetStartTime(
            Seconds(fixedStartTimes[i]));
    }
    appContainer.Stop(appStopTime);*/

    // P2P Gateway ↔ Network Server 
    // p2p.Install(networkServer, gateway)
    Ptr<Node> networkServer = CreateObject<Node>();

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay",   StringValue("2ms"));

    P2PGwRegistration_t gwRegistration;
    for (auto gw = gateways.Begin(); gw != gateways.End(); ++gw)
    {
        // NS primeiro, GW segundo
        auto container       = p2p.Install(networkServer, *gw);
        auto serverP2PNetDev =
            DynamicCast<PointToPointNetDevice>(container.Get(0));
        gwRegistration.emplace_back(serverP2PNetDev, *gw);
    }

    // Network Server 
    nsHelper.SetGatewaysP2P(gwRegistration);
    nsHelper.SetEndDevices(endDevices);
    nsHelper.Install(networkServer);

    // Forwarder
    forHelper.Install(gateways);

    // Simulação 
    // appStopTime + Hours(1) 
    // garante que todos os pacotes em trânsito sejam processados
    Simulator::Stop(appStopTime + Hours(1));

    NS_LOG_INFO("Rodando: " << techniqueName << " L=" << numAntennas);
    Simulator::Run();
        // ── Métricas antes do Destroy ─────────────────────────
    LoraPacketTracker& tracker = loraHelper.GetPacketTracker();

    std::string stats =
        tracker.CountMacPacketsGlobally(
            Seconds(0),
            appStopTime + Hours(1));

    // Parse dos valores MAC
    double sentD = 0.0;
    double receivedD = 0.0;

    std::istringstream iss(stats);
    iss >> sentD >> receivedD;

    uint32_t sent =
        static_cast<uint32_t>(std::round(sentD));

    uint32_t received =
        static_cast<uint32_t>(std::round(receivedD));

    int gwId = gateways.Get(0)->GetId();

    // PHY (diagnóstico)
    std::vector<int> counts = tracker.CountPhyPacketsPerGw(
        Seconds(0),
        appStopTime + Hours(1),
        gwId);

    uint32_t interfered = counts[2];
    uint32_t noMoreGw   = counts[3];
    uint32_t underSens  = counts[4];
    uint32_t lostTx     = counts[5];
    std::cout << tracker.PrintPhyPacketsPerGw(Seconds(0), appStopTime + Hours(1),0) << std::endl;
    std::cout << tracker.CountMacPacketsGlobally(Seconds(0), appStopTime + Hours(1)) << std::endl;

    NS_LOG_INFO("PHY counts size: " << counts.size());
    for (size_t i = 0; i < counts.size(); ++i)
        NS_LOG_INFO("  counts[" << i << "] = " << counts[i]);

    NS_LOG_INFO("MAC Tracker: "
                << sent << " sent, "
                << received << " received");

    NS_LOG_INFO("PHY Tracker: "
                << interfered << " interfered, "
                << noMoreGw << " noMoreGw, "
                << underSens << " underSensitivity, "
                << lostTx << " lostTx");

    double pdr = (sent > 0)
            ? static_cast<double>(received) / sent
            : 0.0;

    double per = 1.0 - pdr;

    double interferenceLossRate = (sent > 0)
        ? static_cast<double>(interfered) / sent
        : 0.0;

    double sensitivityLossRate = (sent > 0)
        ? static_cast<double>(underSens) / sent
        : 0.0;

    double gatewayBusyRate = (sent > 0)
        ? static_cast<double>(noMoreGw) / sent
        : 0.0;

    double txLossRate = (sent > 0)
        ? static_cast<double>(lostTx) / sent
        : 0.0;

    Simulator::Destroy();

    double elapsed = std::chrono::duration<double>(
        Clock::now() - tStart).count();

    SimResult res;
    res.technique  = techniqueName;
    res.L          = numAntennas;
    res.sent       = sent;
    res.received   = received;
    res.elapsedSec = elapsed;
    
    res.interfered = interfered;
    res.noMoreGw = noMoreGw;
    res.underSensitivity = underSens;
    res.lostTx = lostTx;

    res.pdr = pdr;
    res.per = per;

    res.interferenceLossRate = interferenceLossRate;
    res.sensitivityLossRate = sensitivityLossRate;
    res.gatewayBusyRate = gatewayBusyRate;
    res.txLossRate = txLossRate;
    return res;
}

int main(int argc, char* argv[])
{
    int         nDevices       = 30;
    double      radius         = 5000.0;
    double      simTime        = 3600.0;
    int         appPeriod      = 600;    // 1 pacote a cada 600s por ED
    std::string technique      = "ALL";
    uint32_t    nAntennas      = 0;      // 0 = testa 1..5
    uint32_t    seed           = 1;
    bool realisticChannelModel = false; //!< Whether to use a more realistic channel model with
                                    //!< Buildings and correlated shadowing

    CommandLine cmd(__FILE__);
    cmd.AddValue("nDevices",  "Numero de end devices",          nDevices);
    cmd.AddValue("radius",    "Raio da celula (m)",              radius);
    cmd.AddValue("simTime",   "Tempo de simulacao (s)",          simTime);
    cmd.AddValue("appPeriod", "Intervalo entre pacotes (s)",     appPeriod);
    cmd.AddValue("technique", "ALL / SISO / SC / EGC / MRC",    technique);
    cmd.AddValue("nAntennas", "Num antenas (0=todos 1..5)",      nAntennas);
    cmd.AddValue("seed",      "Semente RNG",                     seed);
    cmd.AddValue("realisticChannel", "Whether to use a more realistic channel model",
                 realisticChannelModel);
    cmd.Parse(argc, argv);
    //RngSeedManager::SetSeed(seed);
    //RngSeedManager::SetRun(1);

    LogComponentEnable("LoraNetworkTest", LOG_LEVEL_ALL);

    NS_LOG_INFO("=== Rede LoRa com Combining ===");
    NS_LOG_INFO("nDevices=" << nDevices
                << "  radius=" << radius   << " m"
                << "  simTime=" << simTime << " s"
                << "  appPeriod=" << appPeriod << " s");
    
    std::vector<Vector> fixedPositions;
    std::vector<double> fixedStartTimes;

    Ptr<UniformRandomVariable> startRv =
        CreateObject<UniformRandomVariable>();

    startRv->SetAttribute(
        "Min", DoubleValue(0));

    startRv->SetAttribute(
        "Max",
        DoubleValue(appPeriod));

    for (int i = 0; i < nDevices; ++i)
    {
        fixedStartTimes.push_back(
            startRv->GetValue());
    }
    Ptr<UniformRandomVariable> posRv =
        CreateObject<UniformRandomVariable>();

    Ptr<UniformRandomVariable> angleRv =
        CreateObject<UniformRandomVariable>();

    for (int i = 0; i < nDevices; ++i)
    {
        double r =
            radius * std::sqrt(posRv->GetValue());

        double theta =
            2 * M_PI * angleRv->GetValue();

        double x = r * std::cos(theta);
        double y = r * std::sin(theta);

        fixedPositions.push_back(
            Vector(x, y, 1.2));
    }
    // ── Lista de rodadas ──────────────────────────────────────────
    std::vector<std::string> techs;
    if (technique == "ALL" || technique == "SISO") techs.push_back("SISO");
    if (technique == "ALL" || technique == "SC")   techs.push_back("SC");
    if (technique == "ALL" || technique == "EGC")  techs.push_back("EGC");
    if (technique == "ALL" || technique == "MRC")  techs.push_back("MRC");

    std::vector<uint32_t> antList = (nAntennas == 0)
        ? std::vector<uint32_t>{1, 2, 3, 4, 5}
        : std::vector<uint32_t>{nAntennas};

    // CSV 
    std::ofstream csv("lora_network_pdr_all7.csv");
    csv << "technique,L,"
        << "sent,received,"
        << "interfered,noMoreGw,underSensitivity,lostTx,"
        << "PDR,PER,"
        << "interferenceLossRate,"
        << "sensitivityLossRate,"
        << "gatewayBusyRate,"
        << "txLossRate,"
        << "gain_python_dB,"
        << "elapsed_s\n";
    //csv << "technique,L,sent,received,PDR,PER,gain_python_dB,elapsed_s\n";

    // Cabeçalho terminal
    std::cout << "\n"
          << std::left  << std::setw(8) << "Tecnica"
          << std::right << std::setw(4) << "L"
          << std::setw(8) << "Sent"
          << std::setw(8) << "Recv"
          << std::setw(8) << "PDR"
          << std::setw(8) << "PER"
          << std::setw(10) << "Interf"
          << std::setw(10) << "Sens"
          << std::setw(10) << "Busy"
          << std::setw(10) << "TxLoss"
          << std::setw(12) << "Gain(dB)"
          << std::setw(10) << "Tempo"
          << "\n";
    

    std::vector<SimResult> results;
    double pdrSiso = -1.0;

    for (auto& tech : techs)
    {
        auto list = (tech == "SISO")
                    ? std::vector<uint32_t>{1}
                    : antList;

        for (uint32_t L : list)
        {
            std::cout << "  Rodando " << tech
                      << " L=" << L << " ..." << std::flush;

            SimResult r = RunSimulation(
                            nDevices,
                            radius,
                            simTime,
                            appPeriod,
                            tech,
                            L,
                            seed,
                            realisticChannelModel,
                            fixedPositions,
                            fixedStartTimes);

            results.push_back(r);

            if (tech == "SISO" && L == 1)
                pdrSiso = r.pdr;

            double gainDb = GetPythonGainDb(tech, L);
            
            csv << r.technique << ","
                << r.L << ","
                << r.sent << ","
                << r.received << ","
                << r.interfered << ","
                << r.noMoreGw << ","
                << r.underSensitivity << ","
                << r.lostTx << ","
                << std::fixed << std::setprecision(6)
                << r.pdr << ","
                << r.per << ","
                << r.interferenceLossRate << ","
                << r.sensitivityLossRate << ","
                << r.gatewayBusyRate << ","
                << r.txLossRate << ","
                << gainDb << ","
                << std::setprecision(2)
                << r.elapsedSec
                << "\n";
            csv.flush();
            
            std::cout << "\r"
                    << std::left << std::setw(8) << r.technique
                    << std::right << std::setw(4) << r.L
                    << std::setw(8) << r.sent
                    << std::setw(8) << r.received
                    << std::setw(8) << std::fixed << std::setprecision(3) << r.pdr
                    << std::setw(8) << r.per
                    << std::setw(10) << std::setprecision(3) << r.interferenceLossRate
                    << std::setw(10) << r.sensitivityLossRate
                    << std::setw(10) << r.gatewayBusyRate
                    << std::setw(10) << r.txLossRate
                    << std::setw(12) << std::setprecision(2) << gainDb
                    << std::setw(10) << std::setprecision(1) << r.elapsedSec
                    << "\n" << std::string(110, '-') << "\n";
        }
    }

    csv.close();

    // Ganho de PDR vs SISO 
    if (pdrSiso > 0.0)
    {
        std::cout << "\n=== Ganho vs SISO L=1 ===\n"
                  << std::left  << std::setw(8)  << "Tecnica"
                  << std::right << std::setw(4)  << "L"
                  << std::setw(10) << "PDR"
                  << std::setw(8)  << "PER"
                  << std::setw(12) << "Melhora"
                  << std::setw(16) << "Ganho_Py(dB)"
                  << "\n" << std::string(50, '-') << "\n";

        for (auto& r : results)
        {
            if (r.technique == "SISO") continue;
            double melhora = (r.pdr - pdrSiso) * 100.0;
            double gainDb  = GetPythonGainDb(r.technique, r.L);
            std::cout << std::left  << std::setw(8)  << r.technique
                      << std::right << std::setw(4)  << r.L
                      << std::setw(10) << std::fixed
                      << std::setprecision(3) << r.pdr
                      << std::setw(8)  << std::fixed
                      << std::setprecision(3) << r.per
                      << std::setw(11) << std::setprecision(1)
                      << melhora << "%"
                      << std::setw(15) << std::setprecision(2)
                      << gainDb << " dB\n";
        }
    }

    std::cout << "\nTempo total: " << std::fixed
              << std::setprecision(1) << Elapsed() << " s\n"
              << "✅ lora_network_pdr_all7.csv\n";
    return 0;
}