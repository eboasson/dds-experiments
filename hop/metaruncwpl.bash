if [ "`id -u`" != 0 ] ; then
    echo "this script expects to run with root privileges to mess with priorities and power settings" >&2
    exit 1
fi

function restore() {
    #cpupower frequency-set --governor schedutil -u 10GHz
    cpupower frequency-set --governor schedutil
}
trap restore EXIT

dir=`dirname $0`
renice -5 $$
#cpupower frequency-set --governor performance -u 660MHz
cpupower frequency-set --governor performance

if false ; then
    echo "========= no taskset ========="
    echo "-- network --"
    bash $dir/runcwpl.bash
    echo "--network, lock--"
    bash $dir/runcwpl.bash -l
    echo "--iceoryx--"
    bash $dir/runcwpl.bash -s
    echo "--iceoryx, lock--"
    bash $dir/runcwpl.bash -sl
    echo "--iceoryx, rt:1--"
    bash $dir/runcwpl.bash -sr
    echo "--iceoryx, lock, rt:1--"
    bash $dir/runcwpl.bash -srl
    zip -9 allcores.zip *.txt -x vmstat.txt
    rm *.txt
fi

if false ; then
    echo "========= taskset -c 0-3 (E-cores) ========="
    echo "--network--"
    taskset -c 0-3 bash $dir/runcwpl.bash
    echo "--network, lock--"
    taskset -c 0-3 bash $dir/runcwpl.bash -l
    echo "--iceoryx--"
    taskset -c 0-3 bash $dir/runcwpl.bash -s
    echo "--iceoryx, lock--"
    taskset -c 0-3 bash $dir/runcwpl.bash -sl
    echo "--iceoryx, rt:1--"
    taskset -c 0-3 bash $dir/runcwpl.bash -sr
    echo "--iceoryx, lock, rt:1--"
    taskset -c 0-3 bash $dir/runcwpl.bash -srl
    zip -9 ecores.zip *.txt -x vmstat.txt
    rm *.txt
fi

if true ; then
    echo "========= taskset -c 4-7 (P-cores) ========="
  if true ; then
    echo "--network--"
    taskset -c 4-7 bash $dir/runcwpl.bash
    echo "--network, lock--"
    taskset -c 4-7 bash $dir/runcwpl.bash -l
    echo "--iceoryx--"
    taskset -c 4-7 bash $dir/runcwpl.bash -s
    echo "--iceoryx, lock--"
    taskset -c 4-7 bash $dir/runcwpl.bash -sl
    echo "--iceoryx, rt:1--"
    taskset -c 4-7 bash $dir/runcwpl.bash -sr
  fi
    echo "--iceoryx, lock, rt:1--"
    taskset -c 4-7 bash $dir/runcwpl.bash -srl
    zip -9 pcores.zip *.txt -x vmstat.txt
    rm *.txt
fi
