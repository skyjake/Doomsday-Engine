#!/usr/bin/python
# coding=utf-8

# Script for managing build events.
# - create: tag a new build and prepare the build directory
# - platform_release: run today's platform build and copy the result to the build directory

import sys
import os
import shutil
import time
import glob

FILES_URI = "http://code.iki.fi/builds"

RFC_TIME = "%a, %d %b %Y %H:%M:%S %Z"

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
    os.system("git push --tags")
    
    
def remote_copy(src, dst):
    os.system('scp %s %s' % (src, dst))  
    
    
def find_newest_build():
    newest = None
    for fn in os.listdir(EVENT_DIR):
        if fn[:5] != 'build': continue
        s = os.stat(os.path.join(EVENT_DIR, fn))
        if newest is None or newest[0] < s.st_ctime:
            newest = (s.st_ctime, fn)
    if newest is None:
        return {'tag': None, 'time': time.time()}
    return {'tag': newest[1], 'time': newest[0]}
    
    
def builds_by_time():
    builds = []
    for fn in os.listdir(EVENT_DIR):
        if fn[:5] == 'build':
            s = os.stat(os.path.join(EVENT_DIR, fn))
            builds.append((int(s.st_ctime), fn))
    builds.sort()
    builds.reverse()
    return builds
    
    
def encoded_build_description(name):
    buildDir = os.path.join(EVENT_DIR, name)
    
    msg = ''
    
    # What do we have here?
    files = glob.glob(os.path.join(buildDir, '*.dmg')) + \
            glob.glob(os.path.join(buildDir, '*.exe')) + \
            glob.glob(os.path.join(buildDir, '*.deb'))
            
    files = [os.path.basename(f) for f in files]
    
    if len(files):
        if len(files) > 1:
            s = 's'
        else:
            s = ''
        msg += '<p>%i file%s available:</p><ul>' % (len(files), s)
        for f in files:
            msg += '<li><a href="%s/%s/%s">%s</a></li>' % (FILES_URI, name, f, f)
        msg += '</ul>'
            
    else:
        msg += '<p>No files available for this build.</p>'
    
    # Logs.
    msg += '<h2>Build Logs</h2><ul>'
    for f in glob.glob(os.path.join(buildDir, 'build*txt')):
        os.system('gzip -9c %s > %s' % (f, f+'.gz'))
        f = os.path.basename(f)
        msg += '<li><a href="%s/%s/%s.gz">%s</a></li>' % (FILES_URI, name, f, f)
    msg += '</ul>'
        
    # Changes.
    chgFn = os.path.join(buildDir, 'changes.html')
    if os.path.exists(chgFn):
        msg += '<h2>Revisions</h2>' + file(chgFn, 'rt').read()
    
    return '<![CDATA[' + msg + ']]>'
    

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
    
    prevBuild = find_newest_build()['tag']
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
                 '<a href=\\"http://deng.git.sourceforge.net/git/gitweb.cgi?' + \
                 'p=deng/deng;a=commit;h=%H\\">(show in repository)</a>' + \
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


def update_feed():
    feedName = os.path.join(EVENT_DIR, "events.rss")
    print "Updating feed in %s..." % feedName
    
    out = file(feedName, 'wt')
    print >> out, '<?xml version="1.0" encoding="UTF-8"?>'
    print >> out, '<rss version="2.0">'
    print >> out, '<channel>'
    
    print >> out, '<title>Doomsday Engine Builds</title>'
    print >> out, '<link>http://dengine.net/</link>'
    print >> out, '<description>Automated binary builds of the Doomsday Engine.</description>'
    print >> out, '<language>en-us</language>'
    print >> out, '<webMaster>skyjake@users.sourceforge.net (Jaakko Ker&auml;nen)</webMaster>'
    print >> out, '<pubDate>%s</pubDate>' % time.strftime(RFC_TIME, 
        time.localtime(find_newest_build()['time']))
    print >> out, '<lastBuildDate>%s</lastBuildDate>' % time.strftime(RFC_TIME)
    print >> out, '<generator>dengBot</generator>'
    print >> out, '<ttl>720</ttl>' # 12 hours
    
    for timestamp, tag in builds_by_time():
        print >> out, '<item>'
        print >> out, '<title>Build %s</title>' % tag[5:]
        print >> out, '<author>skyjake@users.sourceforge.net</author>'
        print >> out, '<pubDate>%s</pubDate>' % time.strftime(RFC_TIME, time.localtime(timestamp))
        print >> out, '<description>%s</description>' % encoded_build_description(tag)
        print >> out, '</item>'
    
    # Close.
    print >> out, '</channel>'
    print >> out, '</rss>'


def rebuild_apt_repository():
    aptDir = os.path.join(EVENT_DIR, 'apt')
    print 'Rebuilding the apt repository in %s...' % aptDir
    
    # Empty the apt directory.
    if os.path.exists(aptDir):
        shutil.rmtree(aptDir, True)
    os.mkdir(aptDir)
    
    # Copy the packages (preserving metadata).
    binDir = os.path.join(aptDir, 'binary')
    os.mkdir(binDir)
    for timestamp, tag in builds_by_time():
        for debName in glob.glob(os.path.join(EVENT_DIR, tag, '*.deb')):
            shutil.copy2(debName, os.path.join(binDir, os.path.basename(debName)))
            
    # Generate the apt package index.
    distsDir = os.path.join(aptDir, 'dists/unstable/main/binary-i386')
    os.makedirs(distsDir)
    os.chdir(aptDir)
    os.system("dpkg-scanpackages -a i386 binary /dev/null > %s" % os.path.join(distsDir, 'Packages'))
    os.chdir(distsDir)
    os.system("gzip -9 Packages")
    
    f = file('Release', 'wt')
    print >> f, 'Archive: unstable'
    print >> f, 'Component: main'
    print >> f, 'Origin: skyjake@users.sourceforge.net'
    print >> f, 'Label: Automated Doomsday Engine Builds'
    print >> f, 'Architecture: i386'
    f.close()
    

if sys.argv[1] == 'create':
    create_build_event()

elif sys.argv[1] == 'platform_release':
    todays_platform_release()
    
elif sys.argv[1] == 'feed':
    update_feed()
    
elif sys.argv[1] == 'apt':
    rebuild_apt_repository()
    
else:
    print 'Unknown command:', sys.argv[1]
    sys.exit(1)
