/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2011-2015  Regents of the University of California.
 *
 * This file is part of ndnSIM. See AUTHORS for complete list of ndnSIM authors and
 * contributors.
 *
 * ndnSIM is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * ndnSIM is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * ndnSIM, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 **/

#include "ndn-consumer-cbr.hpp"
#include "ns3/ptr.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/packet.h"
#include "ns3/callback.h"
#include "ns3/string.h"
#include "ns3/boolean.h"
#include "ns3/uinteger.h"
#include "ns3/integer.h"
#include "ns3/double.h"

#include "model/ndn-app-face.hpp"

NS_LOG_COMPONENT_DEFINE("ndn.ConsumerCbr");

namespace ns3 {
namespace ndn {

NS_OBJECT_ENSURE_REGISTERED(ConsumerCbr);

TypeId
ConsumerCbr::GetTypeId(void)
{
  static TypeId tid =
    TypeId("ns3::ndn::ConsumerCbr")
      .SetGroupName("Ndn")
      .SetParent<Consumer>()
      .AddConstructor<ConsumerCbr>()

      .AddAttribute("Frequency", "Frequency of interest packets", StringValue("1.0"),
                    MakeDoubleAccessor(&ConsumerCbr::m_frequency), MakeDoubleChecker<double>())

      .AddAttribute("Randomize",
                    "Type of send time randomization: none (default), uniform, exponential",
                    StringValue("none"),
                    MakeStringAccessor(&ConsumerCbr::SetRandomize, &ConsumerCbr::GetRandomize),
                    MakeStringChecker())

      .AddAttribute("MaxSeq", "Maximum sequence number to request",
                    IntegerValue(std::numeric_limits<uint32_t>::max()),
                    MakeIntegerAccessor(&ConsumerCbr::m_seqMax), MakeIntegerChecker<uint32_t>())

    ;

  return tid;
}

ConsumerCbr::ConsumerCbr()
  : m_frequency(1.0)
  , m_firstTime(true)
  , m_random(0)
{
  NS_LOG_FUNCTION_NOARGS();
  m_seqMax = std::numeric_limits<uint32_t>::max();
}

ConsumerCbr::~ConsumerCbr()
{
  if (m_random)
    delete m_random;
}

void
ConsumerCbr::ScheduleNextPacket()
{
  // double mean = 8.0 * m_payloadSize / m_desiredRate.GetBitRate ();
  // std::cout << "next: " << Simulator::Now().ToDouble(Time::S) + mean << "s\n";

  if (m_firstTime) {
    m_sendEvent = Simulator::Schedule(Seconds(0.0), &Consumer::SendPacket, this);
    m_firstTime = false;
  }
  else if (!m_sendEvent.IsRunning())
    m_sendEvent = Simulator::Schedule((m_random == 0) ? Seconds(1.0 / m_frequency)
                                                      : Seconds(m_random->GetValue()),
                                      &Consumer::SendPacket, this);
}

void
ConsumerCbr::SetRandomize(const std::string& value)
{
  if (m_random)
    delete m_random;

  if (value == "uniform") {
    m_random = new UniformVariable(0.0, 2 * 1.0 / m_frequency);
  }
  else if (value == "exponential") {
    m_random = new ExponentialVariable(1.0 / m_frequency, 50 * 1.0 / m_frequency);
  }
  else
    m_random = 0;

  m_randomType = value;
}

void
ConsumerCbr::SendPacket()
{
  if (!m_active)
    return;

  NS_LOG_FUNCTION_NOARGS();

  uint32_t seq = std::numeric_limits<uint32_t>::max(); // invalid

  while (m_retxSeqs.size()) {
    seq = *m_retxSeqs.begin();
    m_retxSeqs.erase(m_retxSeqs.begin());
    break;
  }

  if (seq == std::numeric_limits<uint32_t>::max()) {
    if (m_seqMax != std::numeric_limits<uint32_t>::max()) {
      if (m_seq >= m_seqMax) {
        return; // we are totally done
      }
    }

    seq = m_seq++;
  }

  //
  shared_ptr<Name> nameWithSequence = make_shared<Name>(m_interestName);
  nameWithSequence->appendSequenceNumber(seq);
  //

  // shared_ptr<Interest> interest = make_shared<Interest> ();
  shared_ptr<Interest> interest = make_shared<Interest>();
  interest->setNonce(m_rand.GetValue());
  interest->setName(*nameWithSequence);
  time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
  interest->setInterestLifetime(interestLifeTime);

  // NS_LOG_INFO ("Requesting Interest: \n" << *interest);
  NS_LOG_INFO("> Interest for " << seq);

  WillSendOutInterest(seq);

  m_transmittedInterests(interest, this, m_face);
  m_face->onReceiveInterest(*interest);

  ScheduleNextPacket();
}

void
ConsumerCbr::OnData(shared_ptr<const Data> data)
{
  Consumer::OnData(data);
}

std::string
ConsumerCbr::GetRandomize() const
{
  return m_randomType;
}

} // namespace ndn
} // namespace ns3
