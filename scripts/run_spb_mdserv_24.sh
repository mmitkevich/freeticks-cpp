DIR=$(dirname $0)/..
BUILD=${BUILD:-relwithdebinfo}
BIN=$DIR/build/$BUILD
USER=${USER:-mmitkevich}
HOST=${HOST:-10.1.110.24}
PCAP_SPB_DIR=/var/lib/data/archive/pcap/spb
RDIR=~/spb_mdserv
ssh $USER@$HOST -t "mkdir -p $RDIR"
scp $DIR/lib/libclickhouse-cpp-lib.so $USER@$HOST:$RDIR
scp $BIN/apps/mdserv $USER@$HOST:$RDIR
scp $DIR/conf/mdserv.json $USER@$HOST:$RDIR
scp $DIR/scripts/run_spb_mdserv.sh $USER@$HOST:$RDIR
scp $DIR/conf/instr-2020-11-02.xml $USER@$HOST:$RDIR
ssh $USER@$HOST -t "cd $RDIR && ln -s $PCAP_SPB_DIR spb"
CMD="./run_spb_mdserv.sh"
echo "RUNNING '$CMD' please see .out, .log and .csv file(s)"
ssh $USER@$HOST -t "cd $RDIR && LD_LIBRARY_PATH=.:$LD_LIBRARY_PATH $CMD"
