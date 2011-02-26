#!/usr/bin/python
# Manages the build events.

import sys
import os
import shutil
import time

EVENT_DIR = sys.argv[1] #/Users/jaakko/Builds
SRC_DIR = sys.argv[2] #/Users/jaakko/Projects/deng

def git_pull():
    """Updates the source with a git pull."""
    print 'Updating source...'
    os.chdir(SRC_DIR)
    os.system("git pull origin master")


def git_tag(tag):
    """Tags the source with a new tag."""
    print 'Tagging with %s...' % tag
    os.chdir(SRC_DIR)
    os.system("git tag %s" % tag)
    
    
def find_newest_build():
    newest = None
    for fn in os.listdir(EVENT_DIR):
        s = os.stat(os.path.join(EVENT_DIR, fn))
        if newest is None or newest[0] < s.st_ctime:
            newest = (s.st_ctime, fn)
    if newest is None:
        return ""
    return newest[1]


def create_build_event():
    """Creates a new build event."""

    git_pull()

    # Identifier/tag for today's build.
    now = time.localtime()
    todaysBuild = 'build' + str((now.tm_year - 2011)*365 + now.tm_yday)
    
    # Tag the source with the build identifier.
    git_tag(todaysBuild)
    
    prevBuild = find_newest_build()
    print 'The previous build is:', prevBuild
    
    if prevBuild == todaysBuild:
        prevBuild = ''
    
    # testing
    #prevBuild = "1.9.0-beta6.8"
    
    buildDir = os.path.join(EVENT_DIR, todaysBuild)

    # Make sure we have a clean directory for this build.
    if os.path.exists(buildDir):
        # Kill it and recreate.
        shutil.rmtree(buildDir, True)
    os.mkdir(buildDir)
    
    if prevBuild:
        # Generate a changelog.
        fn = os.path.join(buildDir, 'changes.html')
        changes = file(fn, 'wt')
        print >> changes, '<ol>'
        
        tmpName = os.path.join(buildDir, 'ctmp')
        
        format = '<li><b>Date:</b> %ai ' + \
                 '<a href=\\"http://deng.git.sourceforge.net/git/gitweb.cgi?p=deng/deng;a=commit;h=%H\\">(show in repository)</a>' + \
                 '<br/><b>Author:</b> %an &lt;<a href=\\"mailto:%ae\\">%ae</a>&gt;<br/>' + \
                 '<blockquote>%B</blockquote>'
        os.system("git log %s..%s --format=\"%s\" >> %s" % (prevBuild, todaysBuild, format, tmpName))

        logText = file(tmpName, 'rt').read()
        print >> changes, logText.replace('\n', '<br/>').replace('</blockquote><br/>', '</blockquote>')

        os.remove(tmpName)

        print >> changes, '</ol>'
        changes.close()
    

create_build_event()

