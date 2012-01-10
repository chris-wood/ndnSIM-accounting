/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011 University of California, Los Angeles
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Alexander Afanasyev <alexander.afanasyev@ucla.edu>
 */

#include "ccnx-consumer-window.h"
#include "ns3/ptr.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/packet.h"
#include "ns3/callback.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"

NS_LOG_COMPONENT_DEFINE ("CcnxConsumerWindow");

namespace ns3
{    
    
NS_OBJECT_ENSURE_REGISTERED (CcnxConsumerWindow);
    
TypeId
CcnxConsumerWindow::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::CcnxConsumerWindow")
    .SetParent<CcnxConsumer> ()
    .AddConstructor<CcnxConsumerWindow> ()

    .AddAttribute ("Window", "Initial size of the window",
                   StringValue ("1000"),
                   MakeUintegerAccessor (&CcnxConsumerWindow::GetWindow, &CcnxConsumerWindow::SetWindow),
                   MakeUintegerChecker<uint32_t> ())
    ;

  return tid;
}

CcnxConsumerWindow::CcnxConsumerWindow ()
  : m_inFlight (0)
{
}

void
CcnxConsumerWindow::SetWindow (uint32_t window)
{
  m_window = window;
}

uint32_t
CcnxConsumerWindow::GetWindow () const
{
  return m_window;
}

void
CcnxConsumerWindow::ScheduleNextPacket ()
{
  if (m_window == 0 || m_inFlight >= m_window)
    {
      if (!m_sendEvent.IsRunning ())
        m_sendEvent = Simulator::Schedule (Seconds (m_rtt->RetransmitTimeout ().ToDouble (Time::S) * 0.1), &CcnxConsumer::SendPacket, this);
      return;
    }
  
  // std::cout << "Window: " << m_window << ", InFlight: " << m_inFlight << "\n";
  m_inFlight++;
  if (!m_sendEvent.IsRunning ())
    m_sendEvent = Simulator::ScheduleNow (&CcnxConsumer::SendPacket, this);
}

///////////////////////////////////////////////////
//          Process incoming packets             //
///////////////////////////////////////////////////

void
CcnxConsumerWindow::OnContentObject (const Ptr<const CcnxContentObjectHeader> &contentObject,
                               const Ptr<const Packet> &payload)
{
  CcnxConsumer::OnContentObject (contentObject, payload);

  m_window = m_window + 1;
  
  if (m_inFlight > 0) m_inFlight--;
  ScheduleNextPacket ();
}

void
CcnxConsumerWindow::OnNack (const Ptr<const CcnxInterestHeader> &interest)
{
  CcnxConsumer::OnNack (interest);
  if (m_inFlight > 0) m_inFlight--;

  if (m_window > 0)
    {
      // m_window = 0.5 * m_window;//m_window - 1;
      m_window = m_window - 1;
    }
}

void
CcnxConsumerWindow::OnTimeout (uint32_t sequenceNumber)
{
  if (m_inFlight > 0) m_inFlight--;
  CcnxConsumer::OnTimeout (sequenceNumber);
}

} // namespace ns3
