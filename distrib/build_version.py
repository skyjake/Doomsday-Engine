# Determining the current Doomsday version and release type from headers.

import os
import string

DOOMSDAY_DIR = os.path.join(os.getcwd(), '..', 'doomsday')
DOOMSDAY_VERSION = "0.0.0-Name"
DOOMSDAY_VERSION_PLAIN = "0.0.0"
DOOMSDAY_RELEASE_TYPE = "Unstable"

def find_version():
    print "Determining Doomsday version...",
    
    versionBase = None
    versionName = None
    releaseType = "Unstable"
    
    f = file(os.path.join(DOOMSDAY_DIR, "engine", "portable", "include", "dd_version.h"), 'rt')
    for line in f.readlines():
        line = line.strip()
        if line[:7] != "#define": continue
        baseAt = line.find("DOOMSDAY_VERSION_BASE")
        nameAt = line.find("DOOMSDAY_RELEASE_NAME")
        typeAt = line.find("DOOMSDAY_RELEASE_TYPE")
        if baseAt > 0:
            versionBase = line[baseAt + 21:].replace('\"','').strip()
        if nameAt > 0:
            versionName = line[nameAt + 21:].replace('\"','').strip()
        if typeAt > 0:
            releaseType = line[typeAt + 21:].replace('\"','').strip()

    global DOOMSDAY_VERSION
    global DOOMSDAY_VERSION_PLAIN
    global DOOMSDAY_RELEASE_TYPE
    
    DOOMSDAY_RELEASE_TYPE = releaseType
    DOOMSDAY_VERSION_PLAIN = versionBase
    DOOMSDAY_VERSION = versionBase
    if versionName:
        DOOMSDAY_VERSION += "-" + versionName    
        
    print DOOMSDAY_VERSION + " (%s)" % releaseType

