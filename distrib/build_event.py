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

BUILD_URI = "http://code.iki.fi/builds"

RFC_TIME = "%a, %d %b %Y %H:%M:%S +0000"

if len(sys.argv) < 4:
    print 'The arguments must be: (command) (eventdir) (distribdir) [localaptdir]'
    sys.exit(1)

EVENT_DIR = sys.argv[2] #/Users/jaakko/Builds
DISTRIB_DIR = sys.argv[3] #/Users/jaakko/Projects/deng
if len(sys.argv) > 4:
    APT_REPO_DIR = sys.argv[4]
else:
    APT_REPO_DIR = ''


def git_checkout(ident):
    print 'Checking out %s...' % ident
    os.chdir(DISTRIB_DIR)
    os.system("git checkout %s" % ident)


def git_pull():
    """Updates the source with a git pull."""
    print 'Updating source...'
    os.chdir(DISTRIB_DIR)
    os.system("git checkout master")
    os.system("git pull")
    
    
def git_tag(tag):
    """Tags the source with a new tag."""
    print 'Tagging with %s...' % tag
    os.chdir(DISTRIB_DIR)
    os.system("git tag %s" % tag)
    os.system("git push --tags")
    
    
def remote_copy(src, dst):
    dst = dst.replace('\\', '/')
    os.system('scp %s %s' % (src, dst))  
    
    
def build_timestamp(tag):
    path = os.path.join(EVENT_DIR, tag)
    oldest = os.stat(path).st_ctime

    for fn in os.listdir(path):
        t = os.stat(os.path.join(path, fn))
        if int(t.st_ctime) < oldest:
            oldest = int(t.st_ctime)
    
    return oldest        
    
    
def find_newest_build():
    newest = None
    for fn in os.listdir(EVENT_DIR):
        if fn[:5] != 'build': continue
        bt = build_timestamp(fn)
        if newest is None or newest[0] < bt:
            newest = (bt, fn)
    if newest is None:
        return {'tag': None, 'time': time.time()}
    return {'tag': newest[1], 'time': newest[0]}
    
    
def builds_by_time():
    builds = []
    for fn in os.listdir(EVENT_DIR):
        if fn[:5] == 'build':
            builds.append((build_timestamp(fn), fn))
    builds.sort()
    builds.reverse()
    return builds
    
    
def html_build_description(name, encoded=True):
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
            if '.dmg' in f:
                platName = "Mac OS X: "
            elif '.exe' in f:
                platName = "Windows: "
            elif '.deb' in f:
                platName = "Ubuntu: "
            else:
                platName = ''
            msg += '<li>%s<a href="%s/%s/%s">%s</a></li>' % (platName, BUILD_URI, name, f, f)
        msg += '</ul>'
            
    else:
        msg += '<p>No files available for this build.</p>'
    
    # Logs.
    msg += '<p><b>Build Logs</b></p><ul>'
    for f in glob.glob(os.path.join(buildDir, 'build*txt')):
        os.system('gzip -9 %s' % f)
    for p in ['darwin', 'win32', 'linux2']:
        msg += '<li>%s:' % p
        files = glob.glob(os.path.join(buildDir, 'build*%s*txt.gz' % p))
        if len(files):
            for f in files:
                f = os.path.basename(f)
                msg += ' <a href="%s/%s/%s">%s</a>' % (BUILD_URI, name, f, f)
        else:
            msg += ' no logs available.'
    msg += '</ul>'
        
    # Changes.
    chgFn = os.path.join(buildDir, 'changes.html')
    if os.path.exists(chgFn):
        msg += '<p><b>Revisions</b></p>' + file(chgFn, 'rt').read()
    
    if encoded: return '<![CDATA[' + msg + ']]>'
    
    return msg
    

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

        if APT_REPO_DIR:
            # Copy also to the appropriate apt directory.
            arch = 'i386'
            if '_amd64' in n: arch = 'amd64'
            remote_copy(os.path.join('releases', n),
                        os.path.join(APT_REPO_DIR, 'dists/unstable/main/binary-%s' % arch, n))
                                 
    # Also the build log.
    remote_copy('buildlog.txt', os.path.join(EVENT_DIR, todays_build_tag(), 
        'buildlog-%s.txt' % sys.platform))
    remote_copy('builderrors.txt', os.path.join(EVENT_DIR, todays_build_tag(), 
        'builderrors-%s.txt' % sys.platform))
                                             
    git_checkout('master')


def write_index_html(tag):
    f = file(os.path.join(EVENT_DIR, tag, 'index.html'), 'wt')
    print >> f, "<html>"
    print >> f, "<head><title>Build %s</title></head>" % tag[5:]
    print >> f, "<body>"
    print >> f, "<h1>Build %s</h1>" % tag[5:]
    print >> f, "<p>The build event was started on %s.</p>" % (time.strftime(RFC_TIME, 
        time.gmtime(build_timestamp(tag))))
    print >> f, html_build_description(tag, False)
    print >> f, "</body>"
    print >> f, "</html>"


def update_feed():
    feedName = os.path.join(EVENT_DIR, "events.rss")
    print "Updating feed in %s..." % feedName
    
    out = file(feedName, 'wt')
    print >> out, '<?xml version="1.0" encoding="UTF-8"?>'
    print >> out, '<rss version="2.0" xmlns:atom="http://www.w3.org/2005/Atom">'
    print >> out, '<channel>'
    
    print >> out, '<title>Doomsday Engine Builds</title>'
    print >> out, '<link>http://dengine.net/</link>'
    print >> out, '<atom:link href="%s/events.rss" rel="self" type="application/rss+xml" />' % BUILD_URI
    print >> out, '<description>Automated binary builds of the Doomsday Engine.</description>'
    print >> out, '<language>en-us</language>'
    print >> out, '<webMaster>skyjake@users.sourceforge.net (Jaakko Keränen)</webMaster>'
    print >> out, '<lastBuildDate>%s</lastBuildDate>' % time.strftime(RFC_TIME, 
        time.gmtime(find_newest_build()['time']))
    print >> out, '<generator>dengBot</generator>'
    print >> out, '<ttl>720</ttl>' # 12 hours
    
    for timestamp, tag in builds_by_time():
        print >> out, '<item>'
        print >> out, '<title>Build %s</title>' % tag[5:]
        print >> out, '<link>%s/%s/</link>' % (BUILD_URI, tag)
        print >> out, '<author>skyjake@users.sourceforge.net (skyjake)</author>'
        print >> out, '<pubDate>%s</pubDate>' % time.strftime(RFC_TIME, time.gmtime(timestamp))
        print >> out, '<description>%s</description>' % html_build_description(tag)
        print >> out, '<guid isPermaLink="false">%s</guid>' % tag
        print >> out, '</item>'
        
        write_index_html(tag)
    
    # Close.
    print >> out, '</channel>'
    print >> out, '</rss>'


def rebuild_apt_repository():
    aptDir = APT_REPO_DIR
    print 'Rebuilding the apt repository in %s...' % aptDir
    
    os.system("apt-ftparchive generate ~/Dropbox/APT/ftparchive.conf")
    os.system("apt-ftparchive -c ~/Dropbox/APT/ftparchive-release.conf release %s/dists/unstable > %s/dists/unstable/Release" % (aptDir, aptDir))
    os.chdir("%s/dists/unstable" % aptDir)
    os.remove("Release.gpg")
    os.system("gpg --output Release.gpg -ba Release")
    os.system("~/Dropbox/Scripts/mirror-tree.py %s %s" % (aptDir, os.path.join(EVENT_DIR, 'apt')))


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
