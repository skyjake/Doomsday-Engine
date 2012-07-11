#!/usr/bin/python
# Determining the number of a build.

import time
import sys

def todays_build():
    now = time.localtime()
    return str((now.tm_year - 2011)*365 + now.tm_yday)
    
if '--print' in sys.argv:
    print todays_build()
