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
import platform
import gzip
import codecs

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
    
    
def count_log_word(fn, word):
    txt = unicode(gzip.open(fn).read(), 'latin1')
    pos = 0
    count = 0
    while True:
        pos = txt.find(unicode(word), pos)
        if pos < 0: break 
        if txt[pos-1] not in ['/', '\\'] and txt[pos+len(word)] != 's':
            count += 1            
        pos += len(word)
    return count
        
    
def count_log_status(fn):
    """Returns tuple of (#warnings, #errors) in the fn."""
    return (count_log_word(fn, 'error'), count_log_word(fn, 'warning'))    
    
    
def count_word(word, inText):
    pos = 0
    count = 0
    while True:
        pos = inText.find(word, pos)
        if pos < 0: break
        count += 1
        pos += len(word)
    return count
    
    
def list_package_files(name):
    buildDir = os.path.join(EVENT_DIR, name)

    files = glob.glob(os.path.join(buildDir, '*.dmg')) + \
            glob.glob(os.path.join(buildDir, '*.exe')) + \
            glob.glob(os.path.join(buildDir, '*.deb'))
            
    return [os.path.basename(f) for f in files]
    

def text_build_summary(name):
    msg = "The build event was started on %s." % (time.strftime(RFC_TIME, 
                                                  time.gmtime(build_timestamp(name))))
    
    msg += ' It'
    
    pkgCount = len(list_package_files(name))
        
    changesName = os.path.join(EVENT_DIR, name, 'changes.html')
    if os.path.exists(changesName):
        msg += " contains %i commits and" % count_word('<li>', file(changesName).read())
        
    msg += " produced %i installable binary package%s." % \
        (pkgCount, 's' if (pkgCount != 1) else '')
    
    return msg
    
        
def html_build_description(name, encoded=True):
    buildDir = os.path.join(EVENT_DIR, name)
    
    msg = '<p>' + text_build_summary(name) + '</p>'
    
    # What do we have here?
    files = list_package_files(name)
    
    # Prepare compiler logs.
    for f in glob.glob(os.path.join(buildDir, 'build*txt')):
        os.system('gzip -9 %s' % f)    
    
    oses = [('Windows', '.exe', ['win32', 'win32-32bit']),
            ('Mac OS X', '.dmg', ['darwin', 'darwin-32bit']),
            ('Ubuntu', 'i386.deb', ['linux2', 'linux2-32bit']),
            ('Ubuntu (64-bit)', 'amd64.deb', ['linux2-64bit'])]
    
    # Print out the matrix.
    msg += '<p><table cellspacing="4" border="0">'
    msg += '<tr style="text-align:left;"><th>OS<th>Binary<th><tt>stdout</tt><th>Err.<th>Warn.<th><tt>stderr</tt><th>Err.<th>Warn.</tr>'
    
    for osName, osExt, osIdent in oses:
        msg += '<tr><td>' + osName + '<td>'
        
        # Find the binary.
        binary = None
        for f in files:
            if osExt in f:
                binary = f
                break
        
        if binary:
            msg += '<a href="%s/%s/%s">%s</a>' % (BUILD_URI, name, f, f)
        else:
            msg += 'n/a'
            
        # Status of the log.
        for logType in ['log', 'errors']:
            logFiles = []
            for osi in osIdent:
                for i in glob.glob(os.path.join(buildDir, 'build%s-%s.txt.gz' % (logType, osi))):
                    logFiles.append(i)
            if len(logFiles) == 0:
                msg += '<td>'
                continue

            # There should only be one.
            f = logFiles[0]

            msg += '<td><a href="%s/%s/%s">txt.gz</a>' % (BUILD_URI, name, os.path.basename(f))
                                
            errors, warnings = count_log_status(f)
            form = '<td bgcolor="%s" style="text-align:center;">'
            if errors > 0:
                msg += form % '#ff4444'
            else:
                msg += form % '#00ee00'
            msg += str(errors)
            
            if warnings > 0:
                msg += form % '#ffee00'
            else:
                msg += form % '#00ee00'
            msg += str(warnings)                

        msg += '</tr>'
    
    msg += '</table></p>'
    
    # Changes.
    chgFn = os.path.join(buildDir, 'changes.html')
    if os.path.exists(chgFn):
        msg += '<p><b>Commits</b></p>' + file(chgFn, 'rt').read()
        
    # Enclose it in a CDATA block if needed.
    if encoded: return '<![CDATA[' + msg + ']]>'    
    return msg
    

def todays_build_tag():
    now = time.localtime()
    return 'build' + str((now.tm_year - 2011)*365 + now.tm_yday)


def collated(s):
    s = s.strip()
    while s[-1] == '.':
        s = s[:-1]
    return s


def update_changes(fromTag=None, toTag=None):
    # Determine automatically?
    if fromTag is None or toTag is None:
        builds = builds_by_time()
        if len(builds) < 2: return
        fromTag = builds[1][1]
        toTag = builds[0][1]

    # Generate a changelog.
    buildDir = os.path.join(EVENT_DIR, toTag)
    fn = os.path.join(buildDir, 'changes.html')
    changes = file(fn, 'wt')
    print >> changes, '<ol>'
    
    tmpName = os.path.join(buildDir, 'ctmp')
    
    format = '{{{{li}}}}{{{{b}}}}[[subjectline]]%s[[/subjectline]]{{{{/b}}}}' + \
             '{{{{br/}}}}by {{{{i}}}}%an{{{{/i}}}} on ' + \
             '%ai ' + \
             '{{{{a href=\\"http://deng.git.sourceforge.net/git/gitweb.cgi?' + \
             'p=deng/deng;a=commit;h=%H\\"}}}}(show in repository){{{{/a}}}}' + \
             '{{{{blockquote}}}}%b{{{{/blockquote}}}}'
    os.system("git log %s..%s --format=\"%s\" >> %s" % (fromTag, toTag, format, tmpName))

    logText = unicode(file(tmpName, 'rt').read(), 'utf-8')
    logText = logText.replace(u'ä', u'&auml;')
    logText = logText.encode('utf-8')
    logText = logText.replace('<', '&lt;')
    logText = logText.replace('>', '&gt;')
    logText = logText.replace('{{{{', '<')
    logText = logText.replace('}}}}', '>')
    
    # Check that the subject lines are not too long.
    MAX_SUBJECT = 100
    pos = 0
    while True:
        pos = logText.find('[[subjectline]]', pos)
        if pos < 0: break
        end = logText.find('[[/subjectline]]', pos)
        
        subject = logText[pos+15:end]    
        extra = ''
        if len(collated(subject)) > MAX_SUBJECT:
            extra = '...' + subject[MAX_SUBJECT:] + ' '
            subject = subject[:MAX_SUBJECT] + '...'
        else:
            # If there is a single dot at the end of the subject, remove it.
            if subject[-1] == '.' and subject[-2] != '.':
                subject = subject[:-1]
        
        # Do the replace.
        logText = logText[:pos] + subject + logText[end+16:]
        
        if len(extra):
            # Place the extra bit in the blockquote.
            bq = logText.find('<blockquote>', pos)
            logText = logText[:bq+12] + extra + logText[bq+12:]            
    
    logText = logText.replace('\n\n', '<br/><br/>').replace('\n', ' ').replace('</blockquote><br/>', '</blockquote>')
    print >> changes, logText

    os.remove(tmpName)

    print >> changes, '</ol>'
    changes.close()


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
        update_changes(prevBuild, todaysBuild)


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
        'buildlog-%s-%s.txt' % (sys.platform, platform.architecture()[0])))
    remote_copy('builderrors.txt', os.path.join(EVENT_DIR, todays_build_tag(), 
        'builderrors-%s-%s.txt' % (sys.platform, platform.architecture()[0])))
                                             
    git_checkout('master')


def write_index_html(tag):
    f = file(os.path.join(EVENT_DIR, tag, 'index.html'), 'wt')
    print >> f, "<html>"
    print >> f, "<head><title>Build %s</title></head>" % tag[5:]
    print >> f, "<body>"
    print >> f, "<h1>Build %s</h1>" % tag[5:]
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
    print >> out, '<ttl>180</ttl>' # 3 hours
    
    for timestamp, tag in builds_by_time():
        print >> out, '<item>'
        print >> out, '<title>Build %s</title>' % tag[5:]
        print >> out, '<link>%s/%s/</link>' % (BUILD_URI, tag)
        print >> out, '<author>skyjake@users.sourceforge.net (skyjake)</author>'
        print >> out, '<pubDate>%s</pubDate>' % time.strftime(RFC_TIME, time.gmtime(timestamp))
        print >> out, '<atom:summary>%s</atom:summary>' % text_build_summary(tag)
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
    
elif sys.argv[1] == 'changes':
    update_changes()
    
else:
    print 'Unknown command:', sys.argv[1]
    sys.exit(1)
