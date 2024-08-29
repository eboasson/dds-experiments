rt=""
shm=false
lock=false
while getopts :rsl OPT; do
    case $OPT in
	r|+r)
	    rt="PRIORITY=rt:1;"
	    ;;
	s|+s)
	    shm=true
	    ;;
	l|+l)
	    lock=true
	    ;;
	*)
	    echo "usage: ${0##*/} [+-rsl} [--] ARGS..."
	    exit 2
    esac
done
shift $(( OPTIND - 1 ))
OPTIND=1

export PATH=/home/erik/install/bin:$PATH
export LD_LIBRARY_PATH=/home/erik/install/lib64${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
export CYCLONEDDS_URI="<Gen><Interf><Netw name=\"lo\" multicast=\"true\"/></></><Tr><Out>cdds.log</></>"
if $shm ; then
    export CYCLONEDDS_URI="$CYCLONEDDS_URI,<Gen><Interf><PubSub name=\"iox\" lib=\"psmx_iox\" config=\"LOG_LEVEL=info;${rt}\"/></></>"
    if pgrep iox-roudi >/dev/null ; then
	echo "roudi is still running, please stop it first" >&2
	exit 1
    fi
fi

function kill_vmstat_on_exit() {
    kill $vmstat
}

#echo "$CYCLONEDDS_URI"
if $shm ; then
    iox-roudi & roudi=$!
    sleep 1
fi

logpre="nw"
lockopt=""
locklog="unl"
if $lock ; then lockopt="-l" ; locklog="l" ; fi
if $shm ; then logpre="shm" ; fi
if [ -n "$rt" ] ; then logpre="${logpre}rt" ; fi
vmstat 7 >vmstat.txt & vmstat=$!
trap kill_vmstat_on_exit EXIT
for i in {0..9} ; do
    ./cwpl -o $logpre-$locklog-$i.txt $lockopt $i & cwpl[$i]=$!
done
sleep 60
kill ${cwpl[@]}
wait ${cwpl[@]}
if $shm ; then
    kill $roudi
fi
kill $vmstat
wait
trap - EXIT

gawk 'NR>3{cs+=$12;us+=$13;sy+=$14;id+=$15;n+=1.0}END{printf "cs,ud,sy,id: {%.0f,%.0f,%.0f,%.0f}\n",cs/n,us/n,sy/n,id/n}' vmstat.txt | tee $logpre-$locklog-vmstat.txt

# if root, make output files owned by me
if [ "`id -u`" = 0 ] ; then
    for i in {0..9} ; do
	chown erik:erik $logpre-$locklog-$i.txt
    done
    chown erik:erik vmstat.txt
fi
