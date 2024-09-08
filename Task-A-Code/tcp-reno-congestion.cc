#include <fstream>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/stats-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpRenoCongestionControl");

class MyApp : public Application
{
public:
  MyApp ();
  virtual ~MyApp ();

  /**
   * Register this type.
   * \return The TypeId.
   */
  static TypeId GetTypeId (void);
  void Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate);
  void ChangeRate(DataRate newRate);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void ScheduleTx (void);
  void SendPacket (void);

  Ptr<Socket>     m_socket;
  Address         m_peer;
  uint32_t        m_packetSize;
  uint32_t        m_nPackets;
  DataRate        m_dataRate;
  EventId         m_sendEvent;
  bool            m_running;
  uint32_t        m_packetsSent;
  DataRate        mDataRate;
};

MyApp::MyApp ()
  : m_socket (0),
    m_peer (),
    m_packetSize (0),
    m_nPackets (0),
    m_dataRate (0),
    m_sendEvent (),
    m_running (false),
    m_packetsSent (0)
{
}

MyApp::~MyApp ()
{
  m_socket = 0;
}

/* static */
TypeId MyApp::GetTypeId (void)
{
  static TypeId tid = TypeId ("MyApp")
    .SetParent<Application> ()
    .SetGroupName ("Tutorial")
    .AddConstructor<MyApp> ()
    ;
  return tid;
}

void
MyApp::Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate)
{
  m_socket = socket;
  m_peer = address;
  m_packetSize = packetSize;
  m_nPackets = nPackets;
  m_dataRate = dataRate;
}

void
MyApp::StartApplication (void)
{
  m_running = true;
  m_packetsSent = 0;
  if (InetSocketAddress::IsMatchingType (m_peer))
    {
      m_socket->Bind ();
    }
  else
    {
      m_socket->Bind6 ();
    }
  m_socket->Connect (m_peer);
  SendPacket ();
}

void
MyApp::StopApplication (void)
{
  m_running = false;

  if (m_sendEvent.IsRunning ())
    {
      Simulator::Cancel (m_sendEvent);
    }

  if (m_socket)
    {
      m_socket->Close ();
    }
}

void
MyApp::SendPacket (void)
{
  Ptr<Packet> packet = Create<Packet> (m_packetSize);
  m_socket->Send (packet);

  if (++m_packetsSent < m_nPackets)
    {
      ScheduleTx ();
    }
}

void
MyApp::ScheduleTx (void)
{
  if (m_running)
    {
      Time tNext (Seconds (m_packetSize * 8 / static_cast<double> (m_dataRate.GetBitRate ())));
      m_sendEvent = Simulator::Schedule (tNext, &MyApp::SendPacket, this);
    }
}

void MyApp::ChangeRate(DataRate newrate) {
	mDataRate = newrate;
	return;
}

static void
CwndChange (Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd)
{
  NS_LOG_UNCOND (Simulator::Now ().GetSeconds () << "\t" << newCwnd);
  *stream->GetStream () << Simulator::Now ().GetSeconds () << " " << oldCwnd << " " << newCwnd << std::endl;
}

// static void
// RxDrop (Ptr<PcapFileWrapper> file, Ptr<const Packet> p)
// {
//   NS_LOG_UNCOND ("RxDrop at " << Simulator::Now ().GetSeconds ());
//   file->Write (Simulator::Now (), p);
// }


void IncRate(Ptr<MyApp> app, DataRate rate) {
	app->ChangeRate(rate);
	return;
}


std::map<Address, double> mapBytesReceived;
std::map<std::string, double> mapBytesReceivedIPV4, mapMaxThroughput;
static double lastTimePrint = 0, lastTimePrintIPV4 = 0;
double printGap = 0;

void ReceivedPacket(Ptr<OutputStreamWrapper> stream, double startTime, std::string context, Ptr<const Packet> p, const Address& addr){
	double timeNow = Simulator::Now().GetSeconds();

	if(mapBytesReceived.find(addr) == mapBytesReceived.end())
		mapBytesReceived[addr] = 0;
	mapBytesReceived[addr] += p->GetSize();
	double kbps_ = (((mapBytesReceived[addr] * 8.0) / 1024)/(timeNow-startTime));
	if(timeNow - lastTimePrint >= printGap) {
		lastTimePrint = timeNow;
		*stream->GetStream() << timeNow-startTime << "\t" <<  kbps_ << std::endl;
	}
}

void ReceivedPacketIPV4(Ptr<OutputStreamWrapper> stream, double startTime, std::string context, Ptr<const Packet> p, Ptr<Ipv4> ipv4, uint32_t interface) {
	double timeNow = Simulator::Now().GetSeconds();

	if(mapBytesReceivedIPV4.find(context) == mapBytesReceivedIPV4.end())
		mapBytesReceivedIPV4[context] = 0;
	if(mapMaxThroughput.find(context) == mapMaxThroughput.end())
		mapMaxThroughput[context] = 0;
	mapBytesReceivedIPV4[context] += p->GetSize();
	double kbps_ = (((mapBytesReceivedIPV4[context] * 8.0) / 1024)/(timeNow-startTime));
	if(timeNow - lastTimePrintIPV4 >= printGap) {
		lastTimePrintIPV4 = timeNow;
		*stream->GetStream() << timeNow-startTime << "\t" <<  kbps_ << std::endl;
		if(mapMaxThroughput[context] < kbps_)
			mapMaxThroughput[context] = kbps_;
	}
}



// code starts from here



Ptr<Socket> flowHelper(Address sinkAddress, Address anyAddress, uint32_t sinkPort, Ptr<Node> senderNode, Ptr<Node> receiverNode, 
					double startTime, double stopTime, uint packetSize, uint32_t numPackets,
					double appStartTime, double appStopTime, uint32_t bitsPerSecond) {

  Config::SetDefault("ns3::TcpL4Protocol::SocketType", ns3::TypeIdValue(ns3::TcpNewReno::GetTypeId()));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", anyAddress);
  ApplicationContainer sinkApps = packetSinkHelper.Install (receiverNode);
  sinkApps.Start (Seconds (startTime));
  sinkApps.Stop (Seconds (stopTime));

  Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(senderNode, TcpSocketFactory::GetTypeId());
	
  uint32_t intDataRate = bitsPerSecond/1024/1024;
  std::string dataRate = std::to_string(intDataRate);
  dataRate += "Mbps";

  std::cout << "datarate: " << dataRate << std::endl;

	Ptr<MyApp> app = CreateObject<MyApp>();
	app->Setup(ns3TcpSocket, sinkAddress, packetSize, numPackets, DataRate (dataRate));
	senderNode->AddApplication(app);
	app->SetStartTime (Seconds (appStartTime));
  app->SetStopTime (Seconds (appStopTime));

	return ns3TcpSocket;
}

int main(int argc, char *argv[]){
    uint32_t numberOfNodes = 5;
    uint32_t numberOfFlows = 5;
    uint32_t packetsPerSecond = 1000;
    uint32_t fileNo = 1;


    uint32_t packetSize = 1.2*1024;

    uint32_t SentPackets = 0;
    uint32_t ReceivedPackets = 0;
    uint32_t LostPackets = 0;


    CommandLine cmd;
    cmd.AddValue("numberOfNodes", "Total number of nodes : ", numberOfNodes);
    cmd.AddValue("numberOfFlows", "Total number of flows : ", numberOfFlows);
    cmd.AddValue("packetsPerSecond", "Packets per second : ", packetsPerSecond);
    cmd.AddValue("fileNo", "File number : ", fileNo);

    cmd.Parse (argc, argv);

    NS_LOG_INFO("Creating nodecontainer for routers, leftnodes and rightnodes");
    NodeContainer routers, leftNodes, rightNodes;
    routers.Create(2);
    leftNodes.Create(numberOfNodes);
    rightNodes.Create(numberOfNodes);


    NS_LOG_INFO("Creating point to point helper");
    PointToPointHelper routerToRouter, routerToNodes;
    routerToNodes.SetChannelAttribute("Delay", StringValue("100us"));
    routerToNodes.SetDeviceAttribute("DataRate", StringValue("10Mbps"));

    routerToRouter.SetChannelAttribute("Delay", StringValue("100us"));
    routerToRouter.SetDeviceAttribute("DataRate", StringValue("10Mbps"));


    Ptr<RateErrorModel> em = CreateObject<RateErrorModel> ();
    em->SetAttribute ("ErrorRate", DoubleValue (0.00001));

    NetDeviceContainer routersDevice, leftDevices, rightDevices, leftRouter, rightRouter;
    routersDevice = routerToRouter.Install(routers);

    for(uint32_t i = 0; i < numberOfNodes; i++){
        NetDeviceContainer tmp = routerToNodes.Install(routers.Get(0), leftNodes.Get(i));
        tmp.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue (em));
        leftRouter.Add(tmp.Get(0));
        leftDevices.Add(tmp.Get(1));
    }

    for(uint32_t i = 0; i < numberOfNodes; i++){
        NetDeviceContainer tmp = routerToNodes.Install(routers.Get(1), rightNodes.Get(i));
        tmp.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue (em));
        rightRouter.Add(tmp.Get(0));
        rightDevices.Add(tmp.Get(1));
    }

    InternetStackHelper stack;
    stack.Install(routers);
    stack.Install(leftNodes);
    stack.Install(rightNodes);



    Ipv4AddressHelper routersIp = Ipv4AddressHelper("172.160.0.0", "255.255.255.0");
    Ipv4AddressHelper leftNodesIp = Ipv4AddressHelper("172.161.0.0", "255.255.255.0");
    Ipv4AddressHelper rightNodesIp = Ipv4AddressHelper("172.162.0.0", "255.255.255.0");


    Ipv4InterfaceContainer routersIFC;
    Ipv4InterfaceContainer leftRouterIFC;
    Ipv4InterfaceContainer rightRouterIFC;
    Ipv4InterfaceContainer leftNodesIFC;
    Ipv4InterfaceContainer rightNodesIFC;

    routersIFC = routersIp.Assign(routersDevice);

    for(uint32_t i = 0; i < numberOfNodes; i++){
        NetDeviceContainer tmpDevice;
        tmpDevice.Add(leftDevices.Get(i));
        tmpDevice.Add(leftRouter.Get(i));

        Ipv4InterfaceContainer tmpInterface = leftNodesIp.Assign(tmpDevice);
        leftNodesIFC.Add(tmpInterface.Get(0));
        leftRouterIFC.Add(tmpInterface.Get(1));

        leftNodesIp.NewNetwork();
    }

    for(uint32_t i = 0; i < numberOfNodes; i++){
        NetDeviceContainer tmpDevice;
        tmpDevice.Add(rightDevices.Get(i));
        tmpDevice.Add(rightRouter.Get(i));

        Ipv4InterfaceContainer tmpInterface = rightNodesIp.Assign(tmpDevice);
        rightNodesIFC.Add(tmpInterface.Get(0));
        rightRouterIFC.Add(tmpInterface.Get(1));

        rightNodesIp.NewNetwork();
    }
  

  /// packet flow

  double durationGap = 15;
	double oneFlowStart = 1;
	double otherFlowStart = 1;
	uint32_t port = 9000;
	uint32_t numPackets = 10000000;
	//std::string transferSpeed = "400Mbps";


  AsciiTraceHelper asciiTraceHelper;

  std::ostringstream stringStream;
  stringStream << "output/congestion-window-file-" << 1 <<  ".cwnd";
	
  Ptr<OutputStreamWrapper> streamCongestionWindow = asciiTraceHelper.CreateFileStream(stringStream.str());


  // flow 1
  Address sinkAddress = InetSocketAddress(rightNodesIFC.GetAddress(0), port);
  Address anyAddress = InetSocketAddress(Ipv4Address::GetAny(), port);

  Ptr<Socket> ns3TcpSocket = flowHelper(sinkAddress, anyAddress, port, leftNodes.Get(0), rightNodes.Get(0),
                                         oneFlowStart, oneFlowStart+durationGap, packetSize, numPackets, 
                                         oneFlowStart, oneFlowStart+durationGap, packetsPerSecond*packetSize);

  ns3TcpSocket->TraceConnectWithoutContext("CongestionWindow", MakeBoundCallback (&CwndChange, streamCongestionWindow));
  
  for(uint32_t i = 1; i < numberOfFlows; i++){

    // flow i+1
    std::ostringstream stringStreamss;
    stringStreamss << "output/congestion-window-file-" << i+1 <<  ".cwnd";
	
    Ptr<OutputStreamWrapper> streamCongestionWindows = asciiTraceHelper.CreateFileStream(stringStreamss.str()); 
    
    sinkAddress = InetSocketAddress(rightNodesIFC.GetAddress(i), port);
    anyAddress = InetSocketAddress(Ipv4Address::GetAny(), port);

    ns3TcpSocket = flowHelper(sinkAddress, anyAddress, port, leftNodes.Get(i), rightNodes.Get(i),
                                         otherFlowStart, otherFlowStart+durationGap, packetSize, numPackets, 
                                         otherFlowStart, otherFlowStart+durationGap, packetsPerSecond*packetSize);
    
    ns3TcpSocket->TraceConnectWithoutContext("CongestionWindow", MakeBoundCallback (&CwndChange, streamCongestionWindows));

  }


  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  int j=0;
  float AvgThroughput = 0;
  Time Delay;

  Ptr<FlowMonitor> flowmon;
	FlowMonitorHelper flowmonHelper;
	flowmon = flowmonHelper.InstallAll();

  Simulator::Stop (Seconds (durationGap + otherFlowStart));
  Simulator::Run ();

  flowmon->CheckForLostPackets();

  std::ostringstream stringStreamForFlow;
  stringStreamForFlow << "output/all-per-flow-file-" << fileNo <<  ".txt";

  std::ostringstream stringStreamNetwork;
  stringStreamNetwork << "output/all-network-file-" << fileNo <<  ".txt";
  

  Ptr<OutputStreamWrapper> streamTP = asciiTraceHelper.CreateFileStream(stringStreamForFlow.str());
  Ptr<OutputStreamWrapper> streamAll = asciiTraceHelper.CreateFileStream(stringStreamNetwork.str());


  *streamTP->GetStream() << "***********Number of Nodes: " << numberOfNodes << "\n";
  *streamTP->GetStream() << "***********Number of Flows: " << numberOfFlows << "\n";
  *streamTP->GetStream() << "***********Packet Per Second: " << packetsPerSecond << "\n";

	Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
	std::map<FlowId, FlowMonitor::FlowStats> stats = flowmon->GetFlowStats();
	for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
		Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);

    *streamTP->GetStream() << "------Flow ID:"  <<i->first << "\n";
    *streamTP->GetStream() << "Src Addr" <<t.sourceAddress << "Dst Addr "<< t.destinationAddress << "\n";
    *streamTP->GetStream() << "Sent Packets=" <<i->second.txPackets << "\n";
    *streamTP->GetStream() << "Received Packets =" <<i->second.rxPackets << "\n";
    *streamTP->GetStream() << "Lost Packets =" <<i->second.txPackets-i->second.rxPackets << "\n";
    *streamTP->GetStream() << "Packet delivery ratio =" <<i->second.rxPackets*100.0/i->second.txPackets << "%" << "\n";
    *streamTP->GetStream() << "Packet loss ratio =" << (i->second.txPackets-i->second.rxPackets)*100.0/i->second.txPackets << "%" << "\n";
    *streamTP->GetStream() << "Delay =" <<i->second.delaySum << "\n";
    *streamTP->GetStream() << "Throughput =" <<i->second.rxBytes * 8.0/(i->second.timeLastRxPacket.GetSeconds()-i->second.timeFirstTxPacket.GetSeconds())/1024<<"Kbps" << "\n";



    SentPackets = SentPackets +(i->second.txPackets);
    ReceivedPackets = ReceivedPackets + (i->second.rxPackets);
    LostPackets = LostPackets + (i->second.txPackets-i->second.rxPackets);
    AvgThroughput = AvgThroughput + (i->second.rxBytes * 8.0/(i->second.timeLastRxPacket.GetSeconds()-i->second.timeFirstTxPacket.GetSeconds())/1024);
    Delay = Delay + (i->second.delaySum);
    
    j = j + 1;

	}

  AvgThroughput = AvgThroughput/j;

  *streamAll->GetStream() << "***********Number of Nodes: " << numberOfNodes << "\n";
  *streamAll->GetStream() << "***********Number of Flows: " << numberOfFlows << "\n";
  *streamAll->GetStream() << "***********Packet Per Second: " << packetsPerSecond << "\n";

  *streamAll->GetStream() << "Total sent packets  =" << SentPackets << "\n";
  *streamAll->GetStream() << "Total Received Packets =" << ReceivedPackets << "\n";
  *streamAll->GetStream() << "Total Lost Packets =" << LostPackets << "\n";
  *streamAll->GetStream() << "Packet Loss ratio =" << ((LostPackets*100.0)/SentPackets)<< "%" << "\n";
  *streamAll->GetStream() << "Packet delivery ratio =" << ((ReceivedPackets*100.0)/SentPackets)<< "%" << "\n";
  *streamAll->GetStream() << "Throughput =" << AvgThroughput<< "Kbps" << "\n";
  *streamAll->GetStream() << "End to End Delay =" << Delay << "\n";
  


// NS_LOG_UNCOND("--------Total Results of the simulation----------"<<std::endl);
// NS_LOG_UNCOND("Total sent packets  =" << SentPackets);
// NS_LOG_UNCOND("Total Received Packets =" << ReceivedPackets);
// NS_LOG_UNCOND("Total Lost Packets =" << LostPackets);
// NS_LOG_UNCOND("Packet Loss ratio =" << ((LostPackets*100)/SentPackets)<< "%");
// NS_LOG_UNCOND("Packet delivery ratio =" << ((ReceivedPackets*100)/SentPackets)<< "%");
// NS_LOG_UNCOND("Average Throughput =" << AvgThroughput<< "Kbps");
// NS_LOG_UNCOND("End to End Delay =" << Delay);
// NS_LOG_UNCOND("End to End Jitter delay =" << Jitter);
// NS_LOG_UNCOND("Total Flod id " << j);

  flowmon->SerializeToXmlFile("output/flow-monfile.flowmon", true, true);




  Simulator::Destroy ();

  return 0;






}