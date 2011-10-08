# Determining the current Doomsday version and release type from headers.

import os
import string

DOOMSDAY_DIR = os.path.join(os.getcwd(), '..', 'doomsday')
DOOMSDAY_VERSION_FULL = "0.0.0-Name"
DOOMSDAY_VERSION_FULL_PLAIN = "0.0.0"
DOOMSDAY_VERSION_MAJOR = 0
DOOMSDAY_VERSION_MINOR = 0
DOOMSDAY_VERSION_REVISION = 0
DOOMSDAY_RELEASE_TYPE = "Unstable"

def find_version():
    print "Determining Doomsday version...",
    
    versionBase = None
    versionName = None
    versionMajor = 0
    versionMinor = 0
    versionRevision = 0
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
            toks = versionBase.split('.')
            versionMajor = toks[0]
            versionMinor = toks[1]
            versionRevision = toks[2]

        if nameAt > 0:
            versionName = line[nameAt + 21:].replace('\"','').strip()
        if typeAt > 0:
            releaseType = line[typeAt + 21:].replace('\"','').strip()

    global DOOMSDAY_VERSION_FULL
    global DOOMSDAY_VERSION_FULL_PLAIN
    global DOOMSDAY_VERSION_MAJOR
    global DOOMSDAY_VERSION_MINOR
    global DOOMSDAY_VERSION_REVISION
    global DOOMSDAY_RELEASE_TYPE
    
    DOOMSDAY_RELEASE_TYPE = releaseType
    DOOMSDAY_VERSION_FULL_PLAIN = versionBase
    DOOMSDAY_VERSION_FULL = versionBase
    if versionName:
        DOOMSDAY_VERSION_FULL += "-" + versionName    
    DOOMSDAY_VERSION_MAJOR = versionMajor
    DOOMSDAY_VERSION_MINOR = versionMinor
    DOOMSDAY_VERSION_REVISION = versionRevision

    print DOOMSDAY_VERSION_FULL + " (%s)" % releaseType
