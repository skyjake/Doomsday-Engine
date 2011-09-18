#!/bin/sh
echo "Generating debian/control..."
ARCH=`dpkg --print-architecture`
sed 's/${Arch}/'$ARCH'/' debian/control.template > debian/control

