#ifndef TcpBR_H
#define TcpBR_H

#include "ns3/tcp-congestion-ops.h"
#include "ns3/tcp-recovery-ops.h"


namespace ns3 {

class TcpBR : public TcpNewReno
{
public:
	//
	static TypeId GetTypeId (void);

	//
	TcpBR (void);

	// Init (tcpnv_init)
	TcpBR (const TcpBR& sock);
	
	// 
	virtual ~TcpBR (void);

	//
	virtual std::string GetName () const;

	virtual void IncreaseWindow (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked);


	virtual uint32_t GetSsThresh (Ptr<const TcpSocketState> tcb, uint32_t bytesInFlight);

	virtual Ptr<TcpCongestionOps> Fork ();
protected:
	virtual uint32_t SlowStart (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked);
	virtual void CongestionAvoidance (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked);
private:
	// TcpBR parameters
    uint32_t m_Reserved_Band; 
	
};
} // namespace ns3
#endif // TcpBR_H