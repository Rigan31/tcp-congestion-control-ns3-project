set terminal png
set output "TcpBR-rtt.png"
set title "Congestion Window Calculation"
set xlabel "time (in seconds)"
set ylabel "RTT"
plot "TcpVariantsComparison-rtt.data" using 1:2 with linespoints title "RTT"