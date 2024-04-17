#!/bin/bash

$1 2>/dev/null 1>/dev/null 0</dev/null &
echo $! > fixture.pid
# Allow server to fully start before lauching clients with ctest
sleep 5
exit 0
