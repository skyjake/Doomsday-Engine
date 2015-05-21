# Parsing the current Doomsday version and release type from headers.

import os, sys, re
import string

DOOMSDAY_DIR = os.path.join(os.path.abspath(os.path.dirname(__file__)), '..', 'doomsday')
    
DOOMSDAY_VERSION_FULL = "0.0.0-Name"
DOOMSDAY_VERSION_FULL_PLAIN = "0.0.0"
DOOMSDAY_VERSION_MAJOR = 0
DOOMSDAY_VERSION_MINOR = 0
DOOMSDAY_VERSION_REVISION = 0
DOOMSDAY_RELEASE_TYPE = "Unstable"

def parse_cmake_for_version(cmakeFile):
    versionMajor = 0
    versionMinor = 0
    versionRevision = 0
    versionName = ""
    releaseType = ""
    
    f = file(cmakeFile, 'rt')
    for line in f.readlines():
        if line[:3] == "set": 
            major = re.search(r'DENG_VERSION_MAJOR.*([0-9]+)', line)
            if major:
                versionMajor = int(major.group(1))
                continue
            minor = re.search(r'DENG_VERSION_MINOR.*([0-9]+)', line)
            if minor:
                versionMinor = int(minor.group(1))
                continue
            patch = re.search(r'DENG_VERSION_PATCH.*([0-9]+)', line)
            if patch:
                versionPatch = int(patch.group(1))
                continue
        else:
            relType = re.search(r'^\s*[^\#]\s*(Unstable|Candidate|Stable)', line)
            if relType:
                releaseType = relType.group(1)
                continue            
            
    return (versionMajor, versionMinor, versionRevision, versionName, releaseType)

def find_version(quiet = False):
    if not quiet: print "Determining Doomsday version...",
    
    versionMajor, versionMinor, versionRevision, versionName, releaseType = \
        parse_cmake_for_version(os.path.join(DOOMSDAY_DIR, 'cmake', 'Version.cmake'))
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


def version_summary(cmakeName):
    import sys
    import build_number
    buildNum = build_number.todays_build()

    # Get the Doomsday version. We can use some of this information.
    find_version(True)
    
    major, minor, revision, name, reltype = parse_cmake_for_version(cmakeName)
    if not reltype: reltype = DOOMSDAY_RELEASE_TYPE
    
    return "%s.%s.%s %s %s %s,%s,%s,%s" % (major, minor, revision, buildNum, reltype,
                                           major, minor, revision, buildNum)

        
# Invoked from qmake? Returns "version_base buildnum releasetype win32_version_with_buildnum"
if __name__ == '__main__':
    print version_summary(sys.argv[1])
