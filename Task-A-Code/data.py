from xml.etree import ElementTree as ET
import sys
et = ET.parse(sys.argv[1])

total_received_bytes = 0
total_sent_bytes = 0
simulation_start = 0.0
simulation_stop = 0.0
delaySum = []
rxPackets = []
txPackets = []
rxBytes = []
txBytes = []
lostPackets = []
flows = []

for flow in et.findall("FlowStats/Flow"):
    total_received_bytes += int(flow.get('rxBytes'))
    total_sent_bytes += int(flow.get('txBytes'))
    tx_start = float(flow.get('timeFirstRxPacket')[:-2])
    tx_stop = float(flow.get('timeLastRxPacket')[:-2])
    delaySum.append(float(flow.get('delaySum')[:-2]))
    flows.append(flow.get('flowId'))
    rxPackets.append(int(flow.get('rxPackets')))
    txPackets.append(int(flow.get('txPackets')))
    rxBytes.append(int(flow.get('rxBytes')))
    txBytes.append(int(flow.get('txBytes')))
    lostPackets.append(int(flow.get('lostPackets')))

    if simulation_start == 0.0:
        simulation_start = tx_start
    elif tx_start < simulation_start:
        simulation_start = tx_start

    if tx_stop > simulation_stop:
        simulation_stop = tx_stop

file0 = open("stat.txt", "w")
file1 = open("endToEnd.txt","w")
file2 = open("deliveryDrop.txt", "w")

file0.write("flowID rxPackets txPackets rxBytes txBytes DrpPackets Delay\n")
for i in range(len(flows)): 
    file0.write("  " + str(flows[i]) + "        " + str(rxPackets[i]) + "   " + str(txPackets[i]) + "      " + str(rxBytes[i]) + "   " + str(txBytes[i]) + "     " + str(lostPackets[i]) + "       " + str(round(delaySum[i]*1e-9, 3)) + "\n")


# file1.write("flowId    End to End Delay\n")
for i in range(len(flows)):
    file1.write(str(flows[i]) + " " + str(round(delaySum[i]*1e-9, 3)) + " s\n")

# file2.write("flowId    Delivery Ratio       Drop Ratio\n")
for i in range(len(flows)):
    file2.write(str(flows[i]) + " " + str(round(rxPackets[i]/txPackets[i], 3)) + " " + str(round(lostPackets[i]/txPackets[i], 3)) + "\n")

totalRxPackets = 0
totalTxPackets = 0
totalLostPackets = 0
totalDelaySum = 0
for i in range(len(flows)):
    totalRxPackets += rxPackets[i]
    totalTxPackets += txPackets[i]
    totalLostPackets += lostPackets[i]
    totalDelaySum += delaySum[i]

print("Total sent bits = " + str(total_sent_bytes*8))
print("Total recieved bits = " + str(total_received_bytes*8))
print("Receiving Packet Start Time = " + str(round(simulation_start*1e-9, 3)))
print("Receiving Packet End Time = " + str(round(simulation_start*1e-9, 3)))

print("Total received packets: " + str(totalRxPackets))
print("Total sent packets: " + str(totalTxPackets))
print("Total Dropped packets: " + str(totalLostPackets))
print("Total End to End delay: " + str(round(totalDelaySum*1e-9, 3)))


duration = (simulation_stop - simulation_start)*1e-9

print("Receiving Packet Duration time = " + str(duration))
print("Network Throughput = " + str(round(total_received_bytes*8/(duration*1e6), 3)) + " Mbps")
print("Delivery Ratio: " + str(round(totalRxPackets/totalTxPackets, 3)) + "\n")
print("Drop Ratio: " + str(round(totalLostPackets/totalTxPackets, 3)) + "\n")

