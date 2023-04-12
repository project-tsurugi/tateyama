#!/bin/sh

APP=build/bench/data_channel_write/data_channel_write
CLIENT_TYPE=mt
#CLIENT_TYPE=mp
MSG_LEN=4192
NLOOP=1000000
MAX_SES_NUM=32

PERF_EVENTS="task-clock,context-switches,cpu-migrations,cycles,instructions,cache-references,cache-misses,branch-instructions,branch-misses"

perf_stat() {
	session_num=$1
	echo "perf stat -e ${PERF_EVENTS} -x , ${APP} ${session_num} ${CLIENT_TYPE} ${MSG_LEN} ${NLOOP}"
	perf stat -e ${PERF_EVENTS} -x , ${APP} ${session_num} ${CLIENT_TYPE} ${MSG_LEN} ${NLOOP}
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
