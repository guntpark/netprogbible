##############################################################

1. make -f Makefile.server
2. make -f Makefile.client
3. tcpdump -X -S -t -n -w filname.pcap -s 1500 -i lo port 55555
4. ./server port
5. ./client localhost port

##############################################################

