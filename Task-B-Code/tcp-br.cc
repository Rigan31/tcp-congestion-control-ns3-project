#include "tcp-br.h"
#include "tcp-socket-state.h"

#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("TcpBR");
NS_OBJECT_ENSURE_REGISTERED (TcpBR);

TypeId
TcpBR::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TcpBR")
    .SetParent<TcpNewReno> ()
    .AddConstructor<TcpBR> ()
    .SetGroupName ("Internet")
    .AddAttribute ("ReservedBand", "Limit on increase",
                   UintegerValue (1),
                   MakeUintegerAccessor (&TcpBR::m_Reserved_Band),
                   MakeUintegerChecker<uint32_t> ())
  ;
  return tid;
}

TcpBR::TcpBR (void)
  : TcpNewReno (),
    m_Reserved_Band (40*1024*1024)
{
  NS_LOG_FUNCTION (this);
}

TcpBR::TcpBR (const TcpBR& sock)
  : TcpNewReno (sock),
    m_Reserved_Band (sock.m_Reserved_Band)
{
  NS_LOG_FUNCTION (this);
}

TcpBR::~TcpBR (void)
{
  NS_LOG_FUNCTION (this);
}

uint32_t
TcpBR::SlowStart (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
{
  NS_LOG_FUNCTION (this << tcb << segmentsAcked);

  if (segmentsAcked >= 1)
    {
      tcb->m_cWnd += tcb->m_segmentSize;
      NS_LOG_INFO ("In SlowStart, updated to cwnd " << tcb->m_cWnd << " ssthresh " << tcb->m_ssThresh);
      return segmentsAcked - 1;
    }

  return 0;
}

void
TcpBR::CongestionAvoidance (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
{
  NS_LOG_FUNCTION (this << tcb << segmentsAcked);

  if (segmentsAcked > 0)
    {
      double adder = static_cast<double> (tcb->m_segmentSize * tcb->m_segmentSize) / tcb->m_cWnd.Get ();
      adder = std::max (1.0, adder);
      tcb->m_cWnd += static_cast<uint32_t> (adder);
      NS_LOG_INFO ("In CongAvoid, updated to cwnd " << tcb->m_cWnd <<
                   " ssthresh " << tcb->m_ssThresh);
    }
}

/**
 * \brief Try to increase the cWnd following the NewReno specification
 *
 * \see SlowStart
 * \see CongestionAvoidance
 *
 * \param tcb internal congestion state
 * \param segmentsAcked count of segments acked
 */
void
TcpBR::IncreaseWindow (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
{
  NS_LOG_FUNCTION (this << tcb << segmentsAcked);

  if (tcb->m_cWnd < tcb->m_ssThresh)
    {
      segmentsAcked = SlowStart (tcb, segmentsAcked);
    }

  if (tcb->m_cWnd >= tcb->m_ssThresh)
    {
      CongestionAvoidance (tcb, segmentsAcked);
    }

  /* At this point, we could have segmentsAcked != 0. This because RFC says
   * that in slow start, we should increase cWnd by min (N, SMSS); if in
   * slow start we receive a cumulative ACK, it counts only for 1 SMSS of
   * increase, wasting the others.
   *
   * // Incorrect assert, I am sorry
   * NS_ASSERT (segmentsAcked == 0);
   */
}

uint32_t
TcpBR::GetSsThresh (Ptr<const TcpSocketState> state,
                         uint32_t bytesInFlight)
{
  NS_LOG_FUNCTION (this << state << bytesInFlight);
  Time tmp = state->m_lastRtt;
  uint32_t RTT = static_cast<uint32_t>(tmp.GetSeconds());
  uint32_t ans = RTT*m_Reserved_Band;
  ans = ans/(1.2*1024);
  ans = ans/8;
  return ans;

  
  // return std::max (2 * state->m_segmentSize, bytesInFlight / 2);
}

std::string
TcpBR::GetName () const
{
  return "TcpBR";
}

Ptr<TcpCongestionOps>
TcpBR::Fork ()
{
  return CopyObject<TcpNewReno> (this);
}

} // namespace ns3

