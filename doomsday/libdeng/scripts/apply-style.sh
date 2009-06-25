#!/bin/sh
find Src -iname '*.c' | xargs ./deng-style.sh
find Include -iname '*.h' | xargs ./deng-header-style.sh

