#!/bin/bash

# This is a Bash script to benchmark the example webservers included in the
# Lua/APR binding on UNIX platforms. This script uses "ab" (ApacheBench).

# Note that the last request sent by ApacheBench may be terminated prematurely
# because of the "ab -t" option. In other words, don't worry about the sporadic
# error message just before or after ApacheBench reports its results.

SECONDS=5 # Number of seconds to run each benchmark.
THREADS=4 # Number of threads to test.

# Benchmark the single threaded webserver.
PORT=$((($RANDOM % 30000) + 1000))
lua ../examples/webserver.lua $PORT &
disown
PID=$!
sleep 5
ab -q -t$SECONDS http://localhost:$PORT/ | grep 'Requests per second\|Transfer rate'
kill $PID
echo

# Benchmark the multi threaded webserver with several thread pool sizes.
for ((i=1; $i<=$THREADS; i+=1)); do
  PORT=$((($RANDOM % 30000) + 1000))
  lua ../examples/threaded-webserver.lua $i $PORT &
  disown
  PID=$!
  sleep 5
  ab -q -t$SECONDS -c$i http://localhost:$PORT/ | grep 'Requests per second\|Transfer rate'
  kill $PID
  echo
done
