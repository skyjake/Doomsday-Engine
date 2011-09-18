#!/usr/bin/python
import time
now = time.localtime()
print str((now.tm_year - 2011)*365 + now.tm_yday)

