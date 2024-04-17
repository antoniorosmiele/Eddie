#!/bin/bash

kill $(cat fixture.pid)
rm fixture.pid
exit 0
