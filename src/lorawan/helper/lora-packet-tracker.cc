/*
 * Copyright (c) 2018 University of Padova
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Davide Magrin <magrinda@dei.unipd.it>
 */

 /*
    PDR (Packet Delivery Ratio)
    pacotes recebidos/interferidos
    perdas por sensibilidade
    perdas por falta de demoduladores
    retransmissões
    delay médio
    estatísticas por gateway e por Spreading Factor (SF)
 */

#include "lora-packet-tracker.h"

#include "ns3/log.h"
#include "ns3/lorawan-mac-header.h"
#include "ns3/simulator.h"

#include <fstream>
#include <iostream>

namespace ns3
{
namespace lorawan
{
NS_LOG_COMPONENT_DEFINE("LoraPacketTracker");

LoraPacketTracker::LoraPacketTracker()
{
    NS_LOG_FUNCTION(this);
}

LoraPacketTracker::~LoraPacketTracker()
{
    NS_LOG_FUNCTION(this);
}

/////////////////
// MAC metrics //
/////////////////

void
LoraPacketTracker::MacTransmissionCallback(Ptr<const Packet> packet, uint8_t sf)
{
    if (IsUplink(packet))
    {
        NS_LOG_INFO("A new packet was sent by the MAC layer");

        MacPacketStatus status;
        status.packet = packet;
        status.sendTime = Simulator::Now();
        status.senderId = Simulator::GetContext();
        status.receivedTime = Time::Max();
		status.sf = sf;

        m_macPacketTracker.insert(std::pair<Ptr<const Packet>, MacPacketStatus>(packet, status));
    }
}

void
LoraPacketTracker::RequiredTransmissionsCallback(uint8_t reqTx,
												 uint8_t sf,
                                                 bool success,
                                                 Time firstAttempt,
                                                 Ptr<Packet> packet)
{
    NS_LOG_INFO("Finished retransmission attempts for a packet");
    NS_LOG_DEBUG("Packet: " << packet << "ReqTx " << unsigned(reqTx) << ", succ: " << success
                            << ", firstAttempt: " << firstAttempt.GetSeconds());

    RetransmissionStatus entry;
    entry.firstAttempt = firstAttempt;
    entry.finishTime = Simulator::Now();
    entry.sf = sf;
    entry.reTxAttempts = reqTx;
    entry.successful = success;

    m_reTransmissionTracker.insert(std::pair<Ptr<Packet>, RetransmissionStatus>(packet, entry));
}

void
LoraPacketTracker::MacGwReceptionCallback(Ptr<const Packet> packet)
{
    if (IsUplink(packet))
    {
        NS_LOG_INFO("A packet was successfully received at the MAC layer of gateway "
                    << Simulator::GetContext());

        // Find the received packet in the m_macPacketTracker
        auto it = m_macPacketTracker.find(packet);
        if (it != m_macPacketTracker.end())
        {
            (*it).second.receptionTimes.insert(
                std::pair<int, Time>(Simulator::GetContext(), Simulator::Now()));
        }
        else
        {
            NS_ABORT_MSG("Packet not found in tracker");
        }
    }
}

/////////////////
// PHY metrics //
/////////////////

void
LoraPacketTracker::TransmissionCallback(Ptr<const Packet> packet, uint32_t edId)
{
    if (IsUplink(packet))
    {
        NS_LOG_INFO("PHY packet " << packet << " was transmitted by device " << edId);
        // Create a packetStatus
        PacketStatus status;
        status.packet = packet;
        status.sendTime = Simulator::Now();
        status.senderId = edId;

        m_packetTracker.insert(std::pair<Ptr<const Packet>, PacketStatus>(packet, status));
    }
}

void
LoraPacketTracker::PacketReceptionCallback(Ptr<const Packet> packet, uint32_t gwId)
{
    if (IsUplink(packet))
    {
        // Remove the successfully received packet from the list of sent ones
        NS_LOG_INFO("PHY packet " << packet << " was successfully received at gateway " << gwId);

        auto it = m_packetTracker.find(packet);
        (*it).second.outcomes.insert(std::pair<int, enum PhyPacketOutcome>(gwId, RECEIVED));
    }
    auto it = m_packetTracker.find(packet);

    if (it == m_packetTracker.end())
    {
        std::cout
            << "RECEPTION: packet nao encontrado no tracker"
            << std::endl;
    }
    static int n = 0;

    if (n < 20)
    {
        std::cout
            << "RX callback gwId="
            << gwId
            << std::endl;
        n++;
    }
}

void
LoraPacketTracker::InterferenceCallback(Ptr<const Packet> packet, uint32_t gwId)
{
    if (IsUplink(packet))
    {
        NS_LOG_INFO("PHY packet " << packet << " was interfered at gateway " << gwId);

        auto it = m_packetTracker.find(packet);
        (*it).second.outcomes.insert(std::pair<int, enum PhyPacketOutcome>(gwId, INTERFERED));
    }
    auto it = m_packetTracker.find(packet);

    if (it == m_packetTracker.end())
    {
        std::cout
            << "RECEPTION: packet nao encontrado no tracker"
            << std::endl;
    }
}

void
LoraPacketTracker::NoMoreReceiversCallback(Ptr<const Packet> packet, uint32_t gwId)
{
    if (IsUplink(packet))
    {
        NS_LOG_INFO("PHY packet " << packet << " was lost because no more receivers at gateway "
                                  << gwId);
        auto it = m_packetTracker.find(packet);
        (*it).second.outcomes.insert(
            std::pair<int, enum PhyPacketOutcome>(gwId, NO_MORE_RECEIVERS));
    }
    auto it = m_packetTracker.find(packet);

    if (it == m_packetTracker.end())
    {
        std::cout
            << "RECEPTION: packet nao encontrado no tracker"
            << std::endl;
    }
}

void
LoraPacketTracker::UnderSensitivityCallback(Ptr<const Packet> packet, uint32_t gwId)
{
    if (IsUplink(packet))
    {
        NS_LOG_INFO("PHY packet " << packet << " was lost because under sensitivity at gateway "
                                  << gwId);

        auto it = m_packetTracker.find(packet);
        (*it).second.outcomes.insert(
            std::pair<int, enum PhyPacketOutcome>(gwId, UNDER_SENSITIVITY));
    }
    auto it = m_packetTracker.find(packet);

    if (it == m_packetTracker.end())
    {
        std::cout
            << "RECEPTION: packet nao encontrado no tracker"
            << std::endl;
    }
}

void
LoraPacketTracker::LostBecauseTxCallback(Ptr<const Packet> packet, uint32_t gwId)
{
    if (IsUplink(packet))
    {
        NS_LOG_INFO(
            "PHY packet " << packet
                          << " was lost because of concurrent downlink transmission at gateway "
                          << gwId);

        auto it = m_packetTracker.find(packet);
        (*it).second.outcomes.insert(std::pair<int, enum PhyPacketOutcome>(gwId, LOST_BECAUSE_TX));
    }
    auto it = m_packetTracker.find(packet);

    if (it == m_packetTracker.end())
    {
        std::cout
            << "RECEPTION: packet nao encontrado no tracker"
            << std::endl;
    }
}

bool
LoraPacketTracker::IsUplink(Ptr<const Packet> packet)
{
    NS_LOG_FUNCTION(this);

    LorawanMacHeader mHdr;
    Ptr<Packet> copy = packet->Copy();
    copy->RemoveHeader(mHdr);
    return mHdr.IsUplink();
}

////////////////////////
// Counting Functions //
////////////////////////

std::vector<int>
LoraPacketTracker::CountPhyPacketsPerGw(Time startTime, Time stopTime, int gwId)
{
    // Vector packetCounts will contain - for the interval given in the input of
    // the function, the following fields: totPacketsSent receivedPackets
    // interferedPackets noMoreGwPackets underSensitivityPackets lostBecauseTxPackets

    std::vector<int> packetCounts(6, 0);

    for (auto itPhy = m_packetTracker.begin(); itPhy != m_packetTracker.end(); ++itPhy)
    {
        if ((*itPhy).second.sendTime >= startTime && (*itPhy).second.sendTime <= stopTime)
        {
            packetCounts.at(0)++;

            NS_LOG_DEBUG("Dealing with packet " << (*itPhy).second.packet);
            NS_LOG_DEBUG("This packet was received by " << (*itPhy).second.outcomes.size()
                                                        << " gateways");
            if ((*itPhy).second.outcomes.count(gwId) == 0)
            {
                NS_LOG_UNCOND("GW " << gwId
                    << " nao encontrou outcome!");
            }

            if ((*itPhy).second.outcomes.count(gwId) > 0)
            {
                switch ((*itPhy).second.outcomes.at(gwId))
                {
                case RECEIVED: { // recebidos
                    packetCounts.at(1)++;
                    break;
                }
                case INTERFERED: { // interferidos - MRC reduz perdas por interferência?
                    packetCounts.at(2)++;
                    break;
                }
                case NO_MORE_RECEIVERS: { // sem receptores disponíveis
                    packetCounts.at(3)++;
                    break;
                }
                case UNDER_SENSITIVITY: { // sob sensibilidade - EGC reduz perdas por sensibilidade?
                    packetCounts.at(4)++;
                    break;
                }
                case LOST_BECAUSE_TX: { // perdidos por causa de transmissão concorrente
                    packetCounts.at(5)++;
                    break;
                }
                case UNSET: { 
                    break;
                }
                }
            }
        }
    }

    return packetCounts;
}

std::string
LoraPacketTracker::PrintPhyPacketsPerGw(Time startTime, Time stopTime, int gwId)
{
    // Vector packetCounts will contain - for the interval given in the input of
    // the function, the following fields: totPacketsSent receivedPackets
    // interferedPackets noMoreGwPackets underSensitivityPackets lostBecauseTxPackets

    std::vector<int> packetCounts(6, 0);

    for (auto itPhy = m_packetTracker.begin(); itPhy != m_packetTracker.end(); ++itPhy)
    {
        if ((*itPhy).second.sendTime >= startTime && (*itPhy).second.sendTime <= stopTime)
        {
            packetCounts.at(0)++;

            NS_LOG_DEBUG("Dealing with packet " << (*itPhy).second.packet);
            NS_LOG_DEBUG("This packet was received by " << (*itPhy).second.outcomes.size()
                                                        << " gateways");

            if ((*itPhy).second.outcomes.count(gwId) > 0)
            {
                switch ((*itPhy).second.outcomes.at(gwId))
                {
                case RECEIVED: {
                    packetCounts.at(1)++;
                    break;
                }
                case INTERFERED: {
                    packetCounts.at(2)++;
                    break;
                }
                case NO_MORE_RECEIVERS: {
                    packetCounts.at(3)++;
                    break;
                }
                case UNDER_SENSITIVITY: {
                    packetCounts.at(4)++;
                    break;
                }
                case LOST_BECAUSE_TX: {
                    packetCounts.at(5)++;
                    break;
                }
                case UNSET: {
                    break;
                }
                }
            }
        }
    }

    std::string output("");
    for (int i = 0; i < 6; ++i)
    {
        output += std::to_string(packetCounts.at(i)) + " ";
    }

    return output;
}

std::string
LoraPacketTracker::CountMacPacketsGlobally(Time startTime, Time stopTime)
{
    NS_LOG_FUNCTION(this << startTime << stopTime);

    double sent = 0;
    double received = 0;
    for (auto it = m_macPacketTracker.begin(); it != m_macPacketTracker.end(); ++it)
    {
        if ((*it).second.sendTime >= startTime && (*it).second.sendTime <= stopTime)
        {
            sent++;
            if (!(*it).second.receptionTimes.empty())
            {
                received++;
            }
        }
    }

    return std::to_string(sent) + " " + std::to_string(received);
}

std::string
LoraPacketTracker::CountMacPacketsGlobally(Time startTime, Time stopTime, uint8_t sf)
{
    NS_LOG_FUNCTION(this << startTime << stopTime);

    double sent = 0;
    double received = 0;
    for (auto it = m_macPacketTracker.begin(); it != m_macPacketTracker.end(); ++it)
    {
		if((*it).second.sf == sf)
		{
	        if ((*it).second.sendTime >= startTime && (*it).second.sendTime <= stopTime)
        	{
            	sent++;
            	if (!(*it).second.receptionTimes.empty())
           		{
                	received++;
            	}
        	}
		}
    }

    return std::to_string(sent) + " " + std::to_string(received);
}


std::string
LoraPacketTracker::CountMacPacketsGloballyCpsr(Time startTime, Time stopTime)
{
    NS_LOG_FUNCTION(this << startTime << stopTime);

    double sent = 0;
    double received = 0;
    for (auto it = m_reTransmissionTracker.begin(); it != m_reTransmissionTracker.end(); ++it)
    {
        if ((*it).second.firstAttempt >= startTime && (*it).second.firstAttempt <= stopTime)
        {
            sent++;
            NS_LOG_DEBUG("Found a packet");
            NS_LOG_DEBUG("Number of attempts: " << unsigned(it->second.reTxAttempts)
                                                << ", successful: " << it->second.successful);
            if (it->second.successful)
            {
                received++;
            }
        }
    }

    return std::to_string(sent) + " " + std::to_string(received);
}

std::string
LoraPacketTracker::CountMacPacketsGloballyCpsr(Time startTime, Time stopTime, uint8_t sf)
{
    NS_LOG_FUNCTION(this << startTime << stopTime);

    double sent = 0;
    double received = 0;
    for (auto it = m_reTransmissionTracker.begin(); it != m_reTransmissionTracker.end(); ++it)
    {
		if((*it).second.sf == sf )
		{
        	if ((*it).second.firstAttempt >= startTime && (*it).second.firstAttempt <= stopTime)
        	{
            	sent++;
            	NS_LOG_DEBUG("Found a packet");
            	NS_LOG_DEBUG("Number of attempts: " << unsigned(it->second.reTxAttempts)
                                                << ", successful: " << it->second.successful);
            	if (it->second.successful)
            	{
               		received++;
            	}
        	}
		}
    }

    return std::to_string(sent) + " " + std::to_string(received);
}

std::string
LoraPacketTracker::CountMacPacketsGloballyDelay (Time startTime, Time stopTime, uint32_t gwId, uint32_t gwNum)
{
  	Time delaySum = Seconds (0);
  	double avgDelay = 0;
  	int packetsOutsideTransient = 0;

	for (uint32_t i = gwId; i < (gwId+gwNum); i++)
	{
  		for (auto itMac = m_macPacketTracker.begin (); itMac != m_macPacketTracker.end (); ++itMac)
    	{
      	 NS_LOG_DEBUG ("Dealing with packet " << (*itMac).first);

      		if ((*itMac).second.sendTime > startTime && (*itMac).second.sendTime < stopTime)
        	{
          		packetsOutsideTransient++;

          		// Compute delays
          		/////////////////
          		if ((*itMac).second.receptionTimes.find(gwId)->second == Time::Max () || (*itMac).second.receptionTimes.find(gwId)->second < (*itMac).second.sendTime )
            	{
              		NS_LOG_DEBUG ("Packet never received, ignoring it");
              		packetsOutsideTransient--;
           	 	}
          		else
            	{
              		delaySum += (*itMac).second.receptionTimes.find(gwId)->second  - (*itMac).second.sendTime;
            	}

        	}
    	}
	}
	//cout << "trans: " << packetsOutsideTransient << " d: " << delaySum.GetSeconds() << endl;

  	if (packetsOutsideTransient != 0)
    {
      avgDelay = (delaySum/packetsOutsideTransient).GetSeconds ();
    }
	
	return(std::to_string(avgDelay));
}

std::string
LoraPacketTracker::CountMacPacketsGloballyDelay (Time startTime, Time stopTime, uint32_t gwId, uint32_t gwNum, uint8_t sf)
{
	Time delaySum = Seconds (0);
  	double avgDelay = 0;
  	int packetsOutsideTransient = 0;

	for (uint32_t i = gwId; i < (gwId+gwNum); i++)
	{
  		for (auto itMac = m_macPacketTracker.begin (); itMac != m_macPacketTracker.end (); ++itMac)
    	{
      	// NS_LOG_DEBUG ("Dealing with packet " << (*itMac).first);
			if ((*itMac).second.sf == sf){
      			if ((*itMac).second.sendTime > startTime && (*itMac).second.sendTime < stopTime)
        		{
          			packetsOutsideTransient++;

          			// Compute delays
          			/////////////////
          			if ((*itMac).second.receptionTimes.find(gwId)->second == Time::Max () || (*itMac).second.receptionTimes.find(gwId)->second < (*itMac).second.sendTime )
            		{
              			// NS_LOG_DEBUG ("Packet never received, ignoring it");
              			packetsOutsideTransient--;
           	 		}
          			else
            		{
              			delaySum += (*itMac).second.receptionTimes.find(gwId)->second  - (*itMac).second.sendTime;
            		}

        		}
			}
    	}
	}
	std::cout << "trans: " << packetsOutsideTransient << " d: " << delaySum.GetSeconds() << std::endl;

  	if (packetsOutsideTransient != 0)
    {
      avgDelay = (delaySum/packetsOutsideTransient).GetSeconds ();
    }
	
	return(std::to_string(avgDelay));

}


} // namespace lorawan
} // namespace ns3
