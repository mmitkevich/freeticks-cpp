build=${build:-relwithdebinfo}
case $1 in
mdconv) ./build/$build/mdconv/mdconv -I spb ~/spb_20201013.pcap
esac
