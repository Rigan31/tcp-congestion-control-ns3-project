#include <fstream>
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/internet-apps-module.h"
#include "ns3/mobility-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/propagation-module.h"
#include "ns3/sixlowpan-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-flow-classifier.h"
#include "ns3/flow-monitor-helper.h"
#include <ns3/lr-wpan-error-model.h>
#include "ns3/stats-module.h"


using namespace ns3;


int main(int argc, char** argv){
    uint32_t numberOfNodes = 5;
    uint32_t numberOfFlows = 5;
    uint32_t packetsPerSecond = 100;
    uint32_t fileNo = 1;
    uint32_t coverageArea = 100;
    uint32_t totalCsmaNodes = 1;


    // uint32_t SentPackets = 0;
    // uint32_t ReceivedPackets = 0;
    // uint32_t LostPackets = 0;



    CommandLine cmd;
    cmd.AddValue("numberOfNodes", "Total number of nodes : ", numberOfNodes);
    cmd.AddValue("numberOfFlows", "Total number of flows : ", numberOfFlows);
    cmd.AddValue("packetsPerSecond", "Packets per second : ", packetsPerSecond);
    cmd.AddValue("coverageArea", "Coverage area : ", coverageArea);
    cmd.AddValue("fileNo", "File number : ", fileNo);

    cmd.Parse (argc, argv);


    Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (ns3::TcpNewReno::GetTypeId()));
    Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (120));
    Config::SetDefault ("ns3::RangePropagationLossModel::MaxRange", DoubleValue (coverageArea));


    Ptr<SingleModelSpectrumChannel> spectrumChannel = CreateObject<SingleModelSpectrumChannel> ();
    Ptr<RangePropagationLossModel> propagationLossModel = CreateObject<RangePropagationLossModel> ();
    Ptr<ConstantSpeedPropagationDelayModel> propagationDelayModel = CreateObject<ConstantSpeedPropagationDelayModel> ();
    spectrumChannel->AddPropagationLossModel (propagationLossModel);
    spectrumChannel->SetPropagationDelayModel (propagationDelayModel);
    LrWpanHelper lrWpanHelper;
    lrWpanHelper.SetChannel(spectrumChannel);



    // as we already created the channel now it's time for nodecontainer

    NodeContainer csmaNodes;
    NodeContainer leftNodes, rightNodes;

    csmaNodes.Create(totalCsmaNodes);
    leftNodes.Create(numberOfNodes);
    rightNodes.Create(numberOfNodes);

    csmaNodes.Add(leftNodes.Get(0));
    csmaNodes.Add(rightNodes.Get(0));


    MobilityHelper mobilityHelper;
    //  we are using constant position mobility helper as our nodes are static
    mobilityHelper.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobilityHelper.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5),
                                 "DeltaY", DoubleValue (10),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));
                              

    // now are are going to install the mobility helper in our wifi devices            
    mobilityHelper.Install(leftNodes);
    mobilityHelper.Install(rightNodes);



    LrWpanHelper leftLrwpanHelper, rightLrwpanHelper;
    NetDeviceContainer leftDevices, rightDevices;

    leftDevices = leftLrwpanHelper.Install (leftNodes);
    leftLrwpanHelper.AssociateToPan (leftDevices, 0);

    rightDevices = rightLrwpanHelper.Install (rightNodes);
    rightLrwpanHelper.AssociateToPan (rightDevices, 0);

    leftLrwpanHelper.SetChannel(spectrumChannel);
    rightLrwpanHelper.SetChannel(spectrumChannel);





    InternetStackHelper ISv6;
    ISv6.Install (leftNodes);
    ISv6.Install (rightNodes);
    ISv6.Install (csmaNodes.Get(0));


    SixLowPanHelper leftSixLowHelper, rightSixLowHelper;
    CsmaHelper csmahelper;

    NetDeviceContainer leftSixLowDevices, rightSixLowDevices;
    NetDeviceContainer csmaDevices;

    leftSixLowDevices = leftSixLowHelper.Install(leftDevices);
    rightSixLowDevices = rightSixLowHelper.Install(rightDevices);
    csmaDevices = csmahelper.Install(csmaNodes);


    Ipv6AddressHelper ipv6;

    ipv6.SetBase (Ipv6Address ("2022:a001::"), Ipv6Prefix (64));
    Ipv6InterfaceContainer leftInterface;
    leftInterface = ipv6.Assign (leftSixLowDevices);
    leftInterface.SetForwarding (0, true);
    leftInterface.SetDefaultRouteInAllNodes (0);

    ipv6.SetBase (Ipv6Address ("2022:a005::"), Ipv6Prefix (64));
    Ipv6InterfaceContainer csmaInterface;
    csmaInterface = ipv6.Assign (csmaDevices);
    csmaInterface.SetForwarding (1, true);
    csmaInterface.SetDefaultRouteInAllNodes (1);

    ipv6.SetBase (Ipv6Address ("2022:a009::"), Ipv6Prefix (64));
    Ipv6InterfaceContainer rightInterface;
    rightInterface = ipv6.Assign (rightSixLowDevices);
    rightInterface.SetForwarding (2, true);
    rightInterface.SetDefaultRouteInAllNodes (2);


    for (uint32_t i = 0; i < leftSixLowDevices.GetN (); i++) {
        Ptr<NetDevice> tmpNetDevice = leftSixLowDevices.Get (i);
        tmpNetDevice->SetAttribute ("UseMeshUnder", BooleanValue (true));
        tmpNetDevice->SetAttribute ("MeshUnderRadius", UintegerValue (10));
    }

    for (uint32_t i = 0; i < rightSixLowDevices.GetN(); i++) {
        Ptr<NetDevice> tmpNetDevice = rightSixLowDevices.Get(i);
        tmpNetDevice->SetAttribute ("UseMeshUnder", BooleanValue (true));
        tmpNetDevice->SetAttribute ("MeshUnderRadius", UintegerValue (10));
    }


    uint32_t ports = 31;

    for( uint32_t i=1; i< numberOfNodes; i++ ) {
        BulkSendHelper sourceApp ("ns3::TcpSocketFactory", Inet6SocketAddress (csmaInterface.GetAddress (0, 1), ports+i));
        sourceApp.SetAttribute ("SendSize", UintegerValue (120));
        ApplicationContainer sourceApps = sourceApp.Install (leftNodes.Get (i));
        sourceApps.Start (Seconds(0));
        sourceApps.Stop (Seconds(100));

        PacketSinkHelper sinkApp ("ns3::TcpSocketFactory", Inet6SocketAddress (Ipv6Address::GetAny (), ports+i));
        sinkApp.SetAttribute ("Protocol", TypeIdValue (TcpSocketFactory::GetTypeId ()));
        ApplicationContainer sinkApps = sinkApp.Install (csmaNodes.Get(0));
        sinkApps.Start (Seconds (0.0));
        sinkApps.Stop (Seconds (100));
        ports++;
    }


    for( uint32_t i=1; i< numberOfNodes; i++ ) {
        BulkSendHelper sourceApp ("ns3::TcpSocketFactory", Inet6SocketAddress (rightInterface.GetAddress (i, 1), ports+i));
        sourceApp.SetAttribute ("SendSize", UintegerValue (120));
        ApplicationContainer sourceApps = sourceApp.Install (csmaNodes.Get (0));
        sourceApps.Start (Seconds(0.0));
        sourceApps.Stop (Seconds(100));

        PacketSinkHelper sinkApp ("ns3::TcpSocketFactory",
        Inet6SocketAddress (Ipv6Address::GetAny (), ports+i));
        sinkApp.SetAttribute ("Protocol", TypeIdValue (TcpSocketFactory::GetTypeId ()));
        ApplicationContainer sinkApps = sinkApp.Install (rightNodes.Get(i));
        sinkApps.Start (Seconds (10.0));
        sinkApps.Stop (Seconds (100));
        
        ports++;
    }


    FlowMonitorHelper flowHelper;
    flowHelper.InstallAll ();

    Simulator::Stop (Seconds (100));
    Simulator::Run ();

    
    std::ostringstream outputVar;
    outputVar << "output/wifi-lowrate-" << fileNo <<  ".xml";
    flowHelper.SerializeToXmlFile (outputVar.str(), false, false);

    Simulator::Destroy ();

  return 0;
}