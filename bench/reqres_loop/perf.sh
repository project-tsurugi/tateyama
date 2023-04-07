#!/bin/sh

APP=build/bench/reqres_loop/reqres_loop
CLIENT_TYPE=mt
#CLIENT_TYPE=mp
MSG_LEN=128
NLOOP=1000000
COMM_TYPE=sync
# COMM_TYPE=async
# COMM_TYPE=nores
MAX_SES_NUM=64


PERF_EVENTS="task-clock,context-switches,cpu-migrations,cycles,instructions,cache-references,cache-misses,branch-instructions,branch-misses"

perf_stat() {
	session_num=$1
	echo "perf stat -e ${PERF_EVENTS} -x , ${APP} ${CLIENT_TYPE} ${session_num} ${MSG_LEN} ${NLOOP} ${COMM_TYPE}"
	perf stat -e ${PERF_EVENTS} -x , ${APP} ${CLIENT_TYPE} ${session_num} ${MSG_LEN} ${NLOOP} ${COMM_TYPE}
	echo ""
	sleep 3
}

perf_stat 1
perf_stat 2
snum=4
while [ $snum -le ${MAX_SES_NUM} ]; do
	perf_stat $snum
	snum=`expr $snum + 4`
done
