# Parsing the current Doomsday version and release type from headers.

import os
import string

DOOMSDAY_DIR = os.path.join(os.getcwd(), '..', 'doomsday')
DOOMSDAY_VERSION_FULL = "0.0.0-Name"
DOOMSDAY_VERSION_FULL_PLAIN = "0.0.0"
DOOMSDAY_VERSION_MAJOR = 0
DOOMSDAY_VERSION_MINOR = 0
DOOMSDAY_VERSION_REVISION = 0
DOOMSDAY_RELEASE_TYPE = "Unstable"

def parse_header_for_version(headerFile):
    versionMajor = 0
    versionMinor = 0
    versionRevision = 0
    versionName = ""
    releaseType = ""
    
    f = file(headerFile, 'rt')
    for line in f.readlines():
        line = line.strip()
        if line[:7] != "#define": continue
        if 'TEXTLONG' in line: continue
        baseAt = line.find("DOOMSDAY_VERSION_BASE")
        baseOff = 21
        if baseAt < 0:
            baseAt = line.find("GAME_VERSION_TEXT")
            baseOff = 17
        if baseAt < 0:
            baseAt = line.find("PLUGIN_VERSION_TEXT")
            baseOff = 20
        nameAt = line.find("DOOMSDAY_RELEASE_NAME")
        typeAt = line.find("DOOMSDAY_RELEASE_TYPE")
        if baseAt > 0:
            versionBase = line[baseAt + baseOff:].replace('\"','').strip()
            toks = versionBase.split('.')
            versionMajor = toks[0]
            versionMinor = toks[1]
            versionRevision = toks[2]
        if nameAt > 0:
            versionName = line[nameAt + 21:].replace('\"','').strip()
        if typeAt > 0:
            releaseType = line[typeAt + 21:].replace('\"','').strip()
            
    return (versionMajor, versionMinor, versionRevision, versionName, releaseType)

def find_version(quiet = False):
    if not quiet: print "Determining Doomsday version...",
    
    versionMajor, versionMinor, versionRevision, versionName, releaseType = \
        parse_header_for_version(os.path.join(DOOMSDAY_DIR, 'engine', 'include', 'dd_version.h'))
    if not releaseType: releaseType = "Unstable"

    versionBase = "%s.%s.%s" % (versionMajor, versionMinor, versionRevision)

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

    if not quiet: print DOOMSDAY_VERSION_FULL + " (%s)" % releaseType

        
# Invoked from qmake? Returns "version_base buildnum releasetype win32_version_with_buildnum"
if __name__ == '__main__':
    import sys
    import build_number
    headerName = sys.argv[1]
    buildNum = build_number.todays_build()

    # Get the Doomsday version. We can use some of this information.
    find_version(True)
    
    major, minor, revision, name, reltype = parse_header_for_version(headerName)
    if not reltype: reltype = DOOMSDAY_RELEASE_TYPE
    
    print "%s.%s.%s %s %s %s,%s,%s,%s" % (major, minor, revision, buildNum, reltype,
                                          major, minor, revision, buildNum)
