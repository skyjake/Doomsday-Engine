# The arguments:
# 1 - output file (+".in" for input file)
# 2 - major
# 3 - minor
# 4 - patch
# 5 - label
# 6 - build number (optional)

import sys
import string
import time

def todays_build():
    now = time.localtime()
    return str((now.tm_year - 2011)*365 + now.tm_yday)

inFile = file(sys.argv[1] + '.in', 'rt')
outFile = file(sys.argv[1], 'wt')

if len(sys.argv) > 6:
    BUILD = sys.argv[6]
else:
    BUILD = todays_build()

VERSION = string.join(sys.argv[2:5], '.')
WIN_VERSION = string.join(sys.argv[2:5], ',') + ',' + BUILD

# Read in the template, substituting symbols for the correct info.
for line in inFile.readlines():
    def quoted(s):
        if line.strip() and line.strip()[0] == '"':
            return '""%s""' % s
        else:
            return '"%s"' % s
            
    line = line \
        .replace('LIBDENG2_VERSION_NUMBER', WIN_VERSION) \
        .replace('LIBDENG2_DESC_WSTR', quoted('Doomsday 2 core library')) \
        .replace('LIBDENG2_DESC', quoted('Doomsday 2 core library')) \
        .replace('LIBDENG2_VERSION_TEXT', quoted(VERSION)) \
        .replace('LIBDENG2_NICENAME', quoted('libcore')) \
        .replace('LIBDENG2_COPYRIGHT', quoted('2004-2017 Deng Team')) \
        .replace('LIBDENG2_FILENAME', quoted('deng_core.dll'))
        
    outFile.write(line)
