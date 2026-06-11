/*
 * Copyright (c) 2017 University of Padova
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Davide Magrin <magrinda@dei.unipd.it>
 */

#ifndef LORA_INTERFERENCE_HELPER_H
#define LORA_INTERFERENCE_HELPER_H

#include "logical-lora-channel.h"

#include "ns3/callback.h"
#include "ns3/nstime.h"
#include "ns3/object.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/traced-callback.h"

#include <list>

namespace ns3
{
namespace lorawan
{

/**
 * \ingroup lorawan
 *
 * Helper for LoraPhy that manages interference calculations.
 *
 * This class keeps a list of signals that are impinging on the antenna of the
 * device, in order to compute which ones can be correctly received and which
 * ones are lost due to interference.
 */
class LoraInterferenceHelper : public Object
{
  public:
    /**
     * A class representing a signal in time.
     *
     * Used in LoraInterferenceHelper to keep track of which signals overlap and
     * cause destructive interference.
     */
    class Event : public SimpleRefCount<LoraInterferenceHelper::Event>
    {
      public:
        /**
         * Construct a new interference signal Event object
         *
         * \param duration The duration in time.
         * \param rxPowerdBm The power of the signal.
         * \param spreadingFactor The modulation spreading factor.
         * \param packet The packet transmitted.
         * \param frequencyMHz The carrier frequency of the signal.
         */
        Event(Time duration,
              double rxPowerdBm,
              uint8_t spreadingFactor,
			  uint8_t irType,
			  uint16_t nodeId,
              Ptr<Packet> packet,
              double frequencyMHz);

        ~Event(); //!< Destructor

        /**
         * Get the duration of the event.
         *
         * \return The duration in time.
         */
        Time GetDuration() const;

        /**
         * Get the starting time of the event.
         *
         * \return The starting time.
         */
        Time GetStartTime() const;

        /**
         * Get the ending time of the event.
         *
         * \return The end time.
         */
        Time GetEndTime() const;

        /**
         * Get the power of the event.
         *
         * \return The power in dBm as a double.
         */
        double GetRxPowerdBm() const;

        /**
         * Get the spreading factor used by this signal.
         *
         * \return The spreading factor value.
         */
        uint8_t GetSpreadingFactor() const;

        /**
         * Get the packet this event was generated for.
         *
         * \return A pointer to the packet.
         */
        Ptr<Packet> GetPacket() const;

        /**
         * Get the frequency this event was on.
         *
         * \return The carrier frequency as a double.
         */
        double GetFrequency() const;
  
		/**
     	* Get the frequency this event was on.
     	*/
    	uint16_t GetNodeId (void) const;

    	/**
     	* Get the incremental redundancy type this event was on.
     	*/
    	uint8_t GetIrType (void) const;

        /**
         * Print the current event in a human readable form.
         *
         * \param stream The output stream to use.
         */
        void Print(std::ostream& stream) const;

      private:
        Time m_startTime;      //!< The time this signal begins (at the device).
        Time m_endTime;        //!< The time this signal ends (at the device).
        uint8_t m_sf;          //!< The spreading factor of this signal.
    	uint8_t m_irType; 	   //!< The incremental redundancy type this event was on.
    	uint16_t m_nodeId;	   //!< The node ID this event was on.
        double m_rxPowerdBm;   //!< The power of this event in dBm (at the device).
        Ptr<Packet> m_packet;  //!< The packet this event was generated for.
        double m_frequencyMHz; //!<  The frequency this event was on.
    };

    enum CombiningMethod
    {
        SINGLE_ANTENNA = 0,
        SC = 1,
        EGC = 2,
        MRC = 3
    };

    uint8_t m_combiningMethod = SINGLE_ANTENNA;
    uint32_t m_numAntennas = 1;

    void SetCombiningMethod(CombiningMethod method);

    CombiningMethod GetCombiningMethod() const;
    
    uint32_t GetNumAntennas() const;

    /**
     * Enumeration of types of collision matrices.
     */
    enum CollisionMatrix
    {
        GOURSAUD,
        ALOHA,
    };
    //void SetCollisionMatrix(CollisionMatrix collisionMatrix);
    void SetCollisionMatrix(enum CollisionMatrix collisionMatrix);
    /**
     * Enumeration of types of incremental redundancy.
    */ 
 	enum IncrementalRedundancy {
    	NOREDUNDANCY,
    	CHASECOMBINING,
		CODEWORDPARITY,
		BUNDLEPARITY,
  	};

    /**
     *  Register this type.
     *  \return The object TypeId.
     */
    static TypeId GetTypeId();

    LoraInterferenceHelper();          //!< Default constructor
    virtual ~LoraInterferenceHelper(); //!< Destructor

    /**
     * Add an event to the InterferenceHelper.
     *
     * \param duration The duration of the packet.
     * \param rxPower The received power in dBm.
     * \param spreadingFactor The spreading factor used by the transmission.
     * \param packet The packet carried by this transmission.
     * \param frequencyMHz The frequency this event was sent at.
     *
     * \return The newly created event.
     */
    Ptr<LoraInterferenceHelper::Event> Add(Time duration,
                                           double rxPower,
                                           uint8_t spreadingFactor,
 										   uint8_t irType,
										   uint16_t nodeId,
                                           Ptr<Packet> packet,
                                           double frequencyMHz);

    /**
     * Get a list of the interferers currently registered at this InterferenceHelper.
     *
     * \return The list of pointers to interference Event objects.
     */
    std::list<Ptr<LoraInterferenceHelper::Event>> GetInterferers();

    /**
     * Print the events that are saved in this helper in a human readable format.
     *
     * \param stream The output stream.
     */
    void PrintEvents(std::ostream& stream);

    /**
     * Determine whether the event was destroyed by interference or not. This is
     * the method where the SNIR tables come into play and the computations
     * regarding power are performed.

     * \param event The event for which to check the outcome.
     * \return The sf of the packets that caused the loss, or 0 if there was no
     * loss.
     */
    
    uint8_t IsDestroyedByInterference(Ptr<LoraInterferenceHelper::Event> event);

    /**
     * Compute the time duration in which two given events are overlapping.
     *
     * \param event1 The first event.
     * \param event2 The second event.
     *
     * \return The overlap time.
     */

    void SetNumAntennas(uint32_t n);
    double GetCombiningGainDb(uint8_t sf) const;
    
    Time GetOverlapTime(Ptr<LoraInterferenceHelper::Event> event1,
                        Ptr<LoraInterferenceHelper::Event> event2);

    /**
     * Delete all events in the LoraInterferenceHelper.
     */
    void ClearAllEvents();

    /**
     * Delete old events in this LoraInterferenceHelper.
     */
    void CleanOldEvents();

  	/**
   	*  in this LoraInterferenceHelper.
   	*/
  	uint8_t GetIncrementalRedundancy (void);

  	/**
   	* Delete index in unordered map in the LoraInterferenceHelper.
   	*/
  	void  ClearIndexUmap(uint16_t idx);

    static CollisionMatrix collisionMatrix; //!< Collision matrix type set by the constructor

    static std::vector<std::vector<double>> collisionSnirAloha;    //!< ALOHA collision matrix
    static std::vector<std::vector<double>> collisionSnirGoursaud; //!< GOURSAUD collision matrix
    
  private:
    /**
     * Set the collision matrix.
     *
     * \param collisionMatrix The type of collision matrix to set.
     *
     * \todo Redundant, only used by constructor which also sets the matrix directly. To be removed.
     */
    //void SetCollisionMatrix(enum CollisionMatrix collisionMatrix);

    std::vector<std::vector<double>> m_collisionSnir; //!< The matrix containing information about
                                                      //!< how packets survive interference
 
  	std::unordered_map<uint16_t, std::vector<std::vector<double>>> m_chaseCombiningSnir;
 
  	uint8_t m_incrementalRed;

    std::list<Ptr<LoraInterferenceHelper::Event>>
        m_events; //!< List of the events this LoraInterferenceHelper is keeping track of
    static Time oldEventThreshold; //!< The threshold after which an event is considered old and
                                   //!< removed from the list
};

/**
 * Allow easy logging of LoraInterferenceHelper Events
 *
 * \param os The output stream for logging
 * \param event The event to be logged
 */
std::ostream& operator<<(std::ostream& os, const LoraInterferenceHelper::Event& event);
} // namespace lorawan

} // namespace ns3
#endif /* LORA_INTERFERENCE_HELPER_H */