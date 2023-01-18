# Reliable Data Transfer

A Layer 4 Transport Layer sender sockets serving in parallel with customized sender window size and buffer size. Implement flow control to avoid data loss. Mimic rount trip time, forward loss rate and return loss rate in real life. Serve maximum goodput speed to 800 Mbps.

## Usage
```
rdt.exe $host $buffer_size $sender_window $rtt $forward_loss_rate $return_loss_rate $speed
```

## Example
```
Main:   sender W = 12000, RTT 0.010 sec, loss 0.0001 / 0, link 1000 Mbps
Main:   initializing DWORD array with 2^28 elements... done in 905 ms
Main:   connected to s3.irl.cs.tamu.edu in 0.052 sec, pkt size 1472 bytes
[  2] B  106921 (156.5 MB) N  108918 T  0 F 12 W 12000 S 626.129 Mbps RTT 0.025
[  4] B  238187 (348.7 MB) N  241957 T  0 F 24 W 12000 S 768.694 Mbps RTT 0.029
[  6] B  376164 (550.7 MB) N  378483 T  0 F 41 W 12000 S 807.993 Mbps RTT 0.028
[  8] B  511608 (749.0 MB) N  515052 T  0 F 62 W 12000 S 793.160 Mbps RTT 0.042
[ 10] B  649055 (950.2 MB) N  650806 T  0 F 75 W 12000 S 804.890 Mbps RTT 0.029
[ 11.296] <-- FIN-ACK 733431 window E8F5B708
Main:   transfer finished in 11.229 sec, 764977.70 Kbps, checksum E8F5B708
Main:   estRTT 0.037, ideal rate 3810620.04 Kbps
```

```
Main:   sender W = 10, RTT 0.010 sec, loss 0.1 / 0, link 1000 Mbps
Main:   initializing DWORD array with 2^20 elements... done in 3 ms
Main:   connected to s3.irl.cs.tamu.edu in 0.040 sec, pkt size 1472 bytes
[  2] B     430 (0.6 MB) N     440 T 16 F 34 W   10 S 2.518 Mbps RTT 0.011
[  4] B     904 (1.3 MB) N     914 T 31 F 81 W   10 S 2.776 Mbps RTT 0.010
[  6] B    1470 (2.2 MB) N    1480 T 44 F 127 W   10 S 3.314 Mbps RTT 0.011
[  8] B    2110 (3.1 MB) N    2120 T 55 F 175 W   10 S 3.748 Mbps RTT 0.011
[ 10] B    2645 (3.9 MB) N    2655 T 70 F 214 W   10 S 3.133 Mbps RTT 0.011
[ 11.238] <-- FIN-ACK 2865 window 5B0360D
Main:   transfer finished in 11.183 sec, 3000.49 Kbps, checksum 5B0360D
Main:   estRTT 0.011, ideal rate 10979.91 Kbps
```

```
Main:   sender W = 2000, RTT 0.010 sec, loss 0 / 0, link 10000 Mbps
Main:   initializing DWORD array with 2^30 elements... done in 3799 ms
Main:   connected to s3.irl.cs.tamu.edu in 0.040 sec, pkt size 1472 bytes
[  2] B  113982 (166.9 MB) N  114804 T  0 F  0 W 2000 S 667.479 Mbps RTT 0.012
[  4] B  245551 (359.5 MB) N  246248 T  0 F  0 W 2000 S 770.210 Mbps RTT 0.011
[  6] B  378961 (554.8 MB) N  379842 T  0 F  0 W 2000 S 781.097 Mbps RTT 0.011
[  8] B  512656 (750.5 MB) N  513429 T  0 F  0 W 2000 S 782.918 Mbps RTT 0.012
[ 10] B  646289 (946.2 MB) N  647032 T  0 F  0 W 2000 S 782.332 Mbps RTT 0.011
[ 12] B  780309 (1142.4 MB) N  781191 T  0 F  0 W 2000 S 784.698 Mbps RTT 0.011
[ 14] B  914911 (1339.4 MB) N  915642 T  0 F  0 W 2000 S 788.229 Mbps RTT 0.011
[ 16] B 1048080 (1534.4 MB) N 1048763 T  0 F  0 W 2000 S 779.838 Mbps RTT 0.011
[ 18] B 1182437 (1731.1 MB) N 1183225 T  0 F  0 W 2000 S 786.718 Mbps RTT 0.011
[ 20] B 1315950 (1926.6 MB) N 1316786 T  0 F  0 W 2000 S 781.852 Mbps RTT 0.012
[ 22] B 1447896 (2119.7 MB) N 1448769 T  0 F  0 W 2000 S 772.635 Mbps RTT 0.011
[ 24] B 1581817 (2315.8 MB) N 1582540 T  0 F  0 W 2000 S 784.241 Mbps RTT 0.011
[ 26] B 1712854 (2507.6 MB) N 1713551 T  0 F  0 W 2000 S 767.353 Mbps RTT 0.011
[ 28] B 1841963 (2696.6 MB) N 1842725 T  0 F  0 W 2000 S 756.062 Mbps RTT 0.011
[ 30] B 1973069 (2888.6 MB) N 1973996 T  0 F  0 W 2000 S 767.757 Mbps RTT 0.011
[ 32] B 2106066 (3083.3 MB) N 2106796 T  0 F  0 W 2000 S 778.830 Mbps RTT 0.012
[ 34] B 2239042 (3278.0 MB) N 2239990 T  0 F  0 W 2000 S 778.514 Mbps RTT 0.011
[ 36] B 2372711 (3473.6 MB) N 2373486 T  0 F  0 W 2000 S 782.766 Mbps RTT 0.011
[ 38] B 2507936 (3671.6 MB) N 2508767 T  0 F  0 W 2000 S 791.678 Mbps RTT 0.010
[ 40] B 2640723 (3866.0 MB) N 2641560 T  0 F  0 W 2000 S 777.601 Mbps RTT 0.011
[ 42] B 2774381 (4061.7 MB) N 2775054 T  0 F  0 W 2000 S 782.572 Mbps RTT 0.011
[ 44] B 2907518 (4256.6 MB) N 2908403 T  0 F  0 W 2000 S 779.557 Mbps RTT 0.011
[ 44.445] <-- FIN-ACK 2933721 window 39F2A0E6
Main:   transfer finished in 44.389 sec, 774059.75 Kbps, checksum 39F2A0E6
Main:   estRTT 0.012, ideal rate 1951823.87 Kbps
```