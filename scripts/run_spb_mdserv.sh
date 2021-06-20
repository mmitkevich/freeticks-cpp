#OUTFILE=/dev/stdout
OUTFILE=spb_mdserv.out
LD_LIBRARY_PATH=.:$LD_LIBRARY_PATH ./mdserv -l spb_mdserv.log -o $OUTFILE -v 5 -m pcap ./mdserv.json