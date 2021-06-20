#OUTFILE=/dev/stdout
OUTFILE=spb_mdserv.out
LOGFILE=spb_mdserv.log
rm $OUTFILE
rm $LOGFILE
rm BestPrice.csv
rm Instruments.csv
LD_LIBRARY_PATH=.:$LD_LIBRARY_PATH ./mdserv -l $LOGFILE -o $OUTFILE -v 5 -m pcap ./mdserv.json
