#!/usr/bin/python
# coding=utf-8

# Script for managing build events.
# - create: tag a new build and prepare the build directory
# - platform_release: run today's platform build and copy the result to the build directory

import sys
import os
import shutil
import time

if len(sys.argv) != 4:
    print 'The arguments must be: (command) (eventdir) (distribdir)'
    sys.exit(1)

EVENT_DIR = sys.argv[2] #/Users/jaakko/Builds
DISTRIB_DIR = sys.argv[3] #/Users/jaakko/Projects/deng


def git_checkout(ident):
    print 'Checking out %s...' % ident
    os.chdir(DISTRIB_DIR)
    os.system("git checkout %s" % ident)


def git_pull():
    """Updates the source with a git pull."""
    print 'Updating source...'
    os.chdir(DISTRIB_DIR)
    os.system("git checkout master")
    os.system("git pull origin master")


def git_tag(tag):
    """Tags the source with a new tag."""
    print 'Tagging with %s...' % tag
    os.chdir(DISTRIB_DIR)
    os.system("git tag %s" % tag)
    
    
def remote_copy(src, dst):
    os.system('scp %s %s' % (src, dst))  
    
    
def find_newest_build():
    newest = None
    for fn in os.listdir(EVENT_DIR):
        s = os.stat(os.path.join(EVENT_DIR, fn))
        if newest is None or newest[0] < s.st_ctime:
            newest = (s.st_ctime, fn)
    if newest is None:
        return ""
    return newest[1]


def todays_build_tag():
    now = time.localtime()
    return 'build' + str((now.tm_year - 2011)*365 + now.tm_yday)


def create_build_event():
    print 'Creating a new build event.'
    git_pull()

    # Identifier/tag for today's build.
    todaysBuild = todays_build_tag()
    
    # Tag the source with the build identifier.
    git_tag(todaysBuild)
    
    prevBuild = find_newest_build()
    print 'The previous build is:', prevBuild
    
    if prevBuild == todaysBuild:
        prevBuild = ''
    
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
        
        format = '<li><b>%s</b>' + \
                 '<br/>by <a href=\\"mailto:%ae\\"><i>%an</i></a> on ' + \
                 '%ai ' + \
                 '<a href=\\"http://deng.git.sourceforge.net/git/gitweb.cgi?p=deng/deng;a=commit;h=%H\\">(show in repository)</a>' + \
                 '<blockquote>%b</blockquote>'
        os.system("git log %s..%s --format=\"%s\" >> %s" % (prevBuild, todaysBuild, format, tmpName))

        logText = unicode(file(tmpName, 'rt').read(), 'utf-8')
        logText = logText.replace(u'ä', u'&auml;')
        logText = logText.encode('utf-8')
        logText = logText.replace('\n', '<br/>').replace('</blockquote><br/>', '</blockquote>')
        print >> changes, logText

        os.remove(tmpName)

        print >> changes, '</ol>'
        changes.close()


def todays_platform_release():
    print "Building today's build."
    git_pull()
    git_checkout(todays_build_tag())
    
    os.chdir(DISTRIB_DIR)
    # We'll copy the new files to the build dir.
    existingFiles = os.listdir('releases')    
    
    print 'platform_release.py...'
    os.system("python platform_release.py > %s 2> %s" % ('buildlog.txt', 'builderrors.txt'))
    
    currentFiles = os.listdir('releases')
    for n in existingFiles:
        currentFiles.remove(n)
        
    for n in currentFiles:
        # Copy any new files.
        remote_copy(os.path.join('releases', n),
                    os.path.join(EVENT_DIR, todays_build_tag(), n))
                                 
    # Also the build log.
    remote_copy('buildlog.txt', os.path.join(EVENT_DIR, todays_build_tag(), 
        'buildlog-%s.txt' % sys.platform))
    remote_copy('builderrors.txt', os.path.join(EVENT_DIR, todays_build_tag(), 
        'builderrors-%s.txt' % sys.platform))
                                             
    git_checkout('master')
    

if sys.argv[1] == 'create':
    create_build_event()

elif sys.argv[1] == 'platform_release':
    todays_platform_release()
    
else:
    print 'Unknown command:', sys.argv[1]
    sys.exit(1)
