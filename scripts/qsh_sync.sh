
#!/bin/bash
_term() { 
  echo "caught SIGTERM DATE=$DATE" 
  kill -TERM "$child" 2>/dev/null
}
trap _term SIGTERM
function dbg {
    [[ $DEBUG == 0 ]] || echo $@
}
function info {
    echo $@
}
function download {
    dbg "start download $1 -> $2"
    curl $CURL_OPTS $1 -o $2
    dbg "done download $1 -> $2"
}
function filter_gt {
    local result=""
    while read line; do [[ $line > $1 ]] && echo $line; done
}
function qsh_sync {
    dbg "qsh_sync URL='$URL' TICKERS='$TICKERS' DATE>'$DATE'"
    dates=$(curl $CURL_OPTS --list-only $URL/|filter_gt "$DATE")
    dbg "dates='$dates'"
    filter=$(echo $TICKERS|sed -e 's/ /|/g')
    dbg "filter='$filter'"
    for date in $dates; do
        DATE=$date
        info "start qsh_sync $date"
        local files=$(curl $CURL_OPTS --list-only $URL/$date/ | egrep "($filter)-.*OrdLog.qsh$")
        dbg "files='$files'"
        [[ $FORCE == 0 ]] || rm -rf "$OUTDIR/$date"
        [[ -d $OUTDIR/$DATE ]] || mkdir -vp $OUTDIR/$date
        for file in $files; do
            outfile=$OUTDIR/$date/$file
            if [ -f $outfile ]; then 
                dbg "cached $outfile"
            else
                download "$URL/$date/$file" $outfile &
            fi
        done
        wait
        info "done qsh_sync $date"
    done
    #echo $files
    #
}
function usage {
    echo "$0 [-vf] -t FORMAT TICKERS..."
}
function parse_args {
    options=$(getopt -o vft: -- "$@")
    [ $? -eq 0 ] || {
        usage
        exit 1
    }
    eval set -- "$options"
    while true; do
        case "$1" in
        -v)
            DEBUG=1
            ;;
        -t)
            shift
            FORMAT=$1
            ;;
        -f)
            FORCE=1
            ;;
        --)
            shift
            break
            ;;
        esac
        shift
    done
}

parse_args $@

CURL_OPTS=--silent

case $FORMAT in
qsh)
    QSH_ZERICH_URL="ftp.zerich.com/pub/Terminals/QScalp/History"
    OUTDIR=qsh
    TICKERS="Si BR CL SILV ED EUR_RUB__TOD EUR_RUB__TOM Eu GOLD MXI RTS"
    DATE="2020-03-01" URL=$QSH_ZERICH_URL qsh_sync
;;
*)
    echo "unknown format $FORMAT"
    exit 1
;;
esac