#!/bin/sh
ARCH=`dpkg --print-architecture`
echo "Generating debian/control..."
sed 's/${Arch}/'$ARCH'/' debian/control.template > debian/control

echo "Generating dsfmod/debian/control..."
sed 's/${Arch}/'$ARCH'/' dsfmod/debian/control.template > dsfmod/debian/control

