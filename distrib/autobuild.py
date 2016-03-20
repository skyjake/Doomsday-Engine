#!/usr/bin/env python2.7
# coding=utf-8
#
# Script for performing automated build events.
# http://dengine.net/dew/index.php?title=Automated_build_system
#
# The Build Pilot (pilot.py) is responsible for task distribution 
# and management.

import sys
import os
import subprocess
import shutil
import time
import string
import glob
import builder
import pickle
import zipfile
from builder.git import * 
from builder.utils import * 
    
    
def pull_from_branch():
    """Pulls commits from the repository."""
    git_pull()
    
    
def create_build_event():
    """Creates and tags a new build for with today's number."""
    print 'Creating a new build event.'
    git_pull()

    # Identifier/tag for today's build.
    todaysBuild = todays_build_tag()
    
    # Tag the source with the build identifier.
    git_tag(todaysBuild)
        
    # Prepare the build directory.
    ev = builder.Event(todaysBuild)
    ev.clean()
    
    # Save the version number and release type.
    import build_version
    build_version.find_version(quiet=True)
    print >> file(ev.file_path('version.txt'), 'wt'), \
        build_version.DOOMSDAY_VERSION_FULL
    print >> file(ev.file_path('releaseType.txt'), 'wt'), \
        build_version.DOOMSDAY_RELEASE_TYPE        

    update_changes()
    

def todays_platform_release():
    """Build today's release for the current platform."""
    print "Building today's build."
    ev = builder.Event()

    git_pull()

    git_checkout(ev.tag() + builder.config.TAG_MODIFIER)
    
    # We'll copy the new files to the build dir.
    os.chdir(builder.config.DISTRIB_DIR)
    oldFiles = DirState('releases', subdirs=False)
    
    try:
        print 'platform_release.py...'
        run_python2("platform_release.py > %s 2> %s" % \
            ('buildlog.txt', 'builderrors.txt'))
    except Exception, x:
        print 'Error during platform_release:', x
            
    for n in DirState('releases', subdirs=False).list_new_files(oldFiles):
        # Copy any new files.
        remote_copy(os.path.join('releases', n), ev.file_path(n))

        if builder.config.APT_REPO_DIR:
            # Copy also to the appropriate apt directory.
            arch = 'i386'
            if '_amd64' in n: arch = 'amd64'
            remote_copy(os.path.join('releases', n),
                        os.path.join(builder.config.APT_REPO_DIR, 
                                     builder.config.APT_DIST + \
                                     '/main/binary-%s' % arch, n))
                                 
    # Also the build logs.
    remote_copy('buildlog.txt', ev.file_path('doomsday-out-%s.txt' % sys_id()))
    remote_copy('builderrors.txt', ev.file_path('doomsday-err-%s.txt' % sys_id()))

    if 'linux' in sys_id():
        remote_copy('dsfmod/fmod-out-%s.txt' % sys_id(), ev.file_path('fmod-out-%s.txt' % sys_id()))
        remote_copy('dsfmod/fmod-err-%s.txt' % sys_id(), ev.file_path('fmod-err-%s.txt' % sys_id()))
                                             
    git_checkout(builder.config.BRANCH)


def sign_packages():
    """Sign all packages in the latest build."""
    ev = builder.Event(latestAvailable=True)
    print "Signing build %i." % ev.number()    
    for fn in os.listdir(ev.path()):
        if fn.endswith('.msi') or fn.endswith('.exe') or fn.endswith('.dmg') or fn.endswith('.deb'):
            # Make a signature for this.
            os.system("gpg --output %s -ba %s" % (ev.file_path(fn) + '.sig', ev.file_path(fn)))


def publish_packages():
    """Publish all packages to SourceForge."""
    ev = builder.Event(latestAvailable=True)
    print "Publishing build %i." % ev.number()
    system_command('deng_copy_build_to_sourceforge.sh "%s"' % ev.path())
    

def find_previous_tag(toTag, version):
    """Finds the build tag preceding `toTag`.
    
    Arguments:
        toTag:   Build tag ("buildNNNN").
        version: The preceding build tag must be from this version, 
                 comparing only major and minor version components.
                 Set to None to omit comparisons based on version.                 
    Returns:
        Build tag ("buildNNNN"). 
        None, if there is no applicable previous build. 
    """
    builds = builder.events_by_time() # descending by timestamp
    
    print "Finding previous build for %s (version:%s)" % (toTag, version)
         
    # Disregard builds later than `toTag`.
    while len(builds) and builds[0][1].tag() != toTag:
        del builds[0]
    if len(builds): del builds[0] # == toTag        
    if len(builds) == 0:
        return None
    
    for timestamp, ev in builds:
        print ev.tag(), ev.version(), time.strftime('%Y-%m-%d %H:%M:%S', time.gmtime(ev.timestamp()))
        
        if version is None:
            # Anything goes.
            return ev.tag()
        else:
            requiredVer = version_split(version)
            eventVer = version_split(ev.version())
            if requiredVer[:2] == eventVer[:2]:
                return ev.tag()            
        
    # Nothing suitable found. Fall back to a more lax search.
    return find_previous_tag(toTag, None)


def update_changes(debChanges=False):
    """Generates the list of commits for the latest build."""

    git_pull()
    toTag = todays_build_tag()

    import build_version

    # Look up the relevant version.
    event = builder.Event(toTag)
    refVersion = event.version()

    # Let's find the previous event of this version.
    fromTag = find_previous_tag(toTag, refVersion)
    
    if fromTag is None or toTag is None:
        # Range not defined.
        return

    print 'Changes for range', fromTag, '..', toTag

    changes = builder.Changes(fromTag, toTag)

    if debChanges:
        # Only update the Debian changelog.
        changes.generate('deb')

        # Also update the doomsday-fmod changelog (just version number).
        os.chdir(os.path.join(builder.config.DISTRIB_DIR, 'dsfmod'))
        
        fmodVer = build_version.parse_cmake_for_version('../../doomsday/cmake/Version.cmake')
        debVer = "%s.%s.%s-%s" % (fmodVer[0], fmodVer[1], fmodVer[2], todays_build_tag())
        print "Marking new FMOD version:", debVer
        msg = 'New release: Doomsday Engine build %i.' % builder.Event().number()
        os.system('rm -f debian/changelog && dch --check-dirname-level 0 --create --package doomsday-fmod -v %s "%s"' % (debVer, msg))
        os.system('dch --release ""')

    else:
        changes.generate('html')
        changes.generate('xml')
           
           
def update_debian_changelog():
    """Updates the Debian changelog at (distrib)/debian/changelog."""
    # Update debian changelog.
    update_changes(debChanges=True)
           

def build_source_package():
    """Builds the source tarball and a Debian source package."""
    update_debian_changelog()
    git_pull()
    ev = builder.Event(latestAvailable=True)
    print "Creating source tarball for build %i." % ev.number()
    os.chdir(os.path.join(builder.config.DISTRIB_DIR))
    remkdir('srcwork')
    os.chdir('srcwork')
    pkgName = 'doomsday'
    if ev.release_type() == 'stable':
        print 'Stable packages will be prepared'
        system_command('deng_package_source.sh ' + ev.version_base())
        pkgName += '-stable'
    else:
        system_command('deng_package_source.sh build %i %s' % (ev.number(), 
                                                               ev.version_base()))
    for fn in os.listdir('.'):
        if fn[:9] == 'doomsday-' and fn[-7:] == '.tar.gz' and ev.version_base() in fn:
            remote_copy(fn, ev.file_path(fn))
            break

    # Check distribution.
    system_command("lsb_release -a | perl -n -e 'm/Codename:\s(.+)/ && print $1' > /tmp/distroname")
    distro = file('/tmp/distroname', 'rt').read()

    # Create a source Debian package and upload it to Launchpad.
    pkgVer = '%s.%i+%s' % (ev.version_base(), ev.number(), distro)
    pkgDir = pkgName + '-%s' % pkgVer

    print "Extracting", fn
    system_command('tar xzf %s' % fn)
    print "Renaming", fn[:-7], 'to', pkgDir + '.orig'
    os.rename(fn[:-7], pkgDir + '.orig')

    origName = pkgName + '_%s' % ev.version_base() + '.orig.tar.gz'
    print "Symlink to", origName
    system_command('ln %s %s' % (fn, origName))

    print "Extracting", fn
    system_command('tar xzf %s' % fn)
    print "Renaming", fn[:-7], 'to', pkgDir
    os.rename(fn[:-7], pkgDir)
    os.chdir(pkgDir)
    system_command('echo "" | dh_make --yes -s -c gpl2 --file ../%s' % fn)
    os.chdir('debian')
    for fn in os.listdir('.'):
        if fn[-3:].lower() == '.ex': os.remove(fn)
    os.remove('README.Debian')
    os.remove('README.source')
    
    def gen_changelog(src, dst, extraSub=''):
        system_command("sed 's/%s-build%i/%s/;%s' %s > %s" % (
                ev.version_base(), ev.number(), pkgVer, extraSub, src, dst))

    # Figure out the name of the distribution.
    dsub = ''
    if distro:
        dsub = 's/) precise;/) %s;/' % distro
    if pkgName != 'doomsday':
        if dsub: dsub += ';'
        dsub += 's/^doomsday /%s /' % pkgName

    gen_changelog('../../../debian/changelog', 'changelog', dsub)
    system_command("sed 's/${Arch}/i386 amd64/;s/${Package}/%s/' ../../../debian/control.template > control" % pkgName)
    system_command("sed 's/`..\/build_number.py --print`/%i/;s/..\/..\/doomsday/..\/doomsday/' ../../../debian/rules > rules" % ev.number())
    os.chdir('..')
    system_command('debuild -S')
    os.chdir('..')
    system_command('dput ppa:sjke/doomsday %s_%s_source.changes' % (pkgName, pkgVer))


def rebuild_apt_repository():
    """Rebuilds the Apt repository by running apt-ftparchive."""
    aptDir = builder.config.APT_REPO_DIR
    print 'Rebuilding the apt repository in %s...' % aptDir
    
    os.system("apt-ftparchive generate ~/Dropbox/APT/ftparchive.conf")
    os.system("apt-ftparchive -c %s release %s/%s > %s/%s/Release" % (builder.config.APT_CONF_FILE, aptDir, builder.config.APT_DIST, aptDir, builder.config.APT_DIST))
    os.chdir("%s/%s" % (aptDir, builder.config.APT_DIST))
    try:
        os.remove("Release.gpg")
    except OSError:
        # Never mind.
        pass
    os.system("gpg --output Release.gpg -ba Release")
    os.system("~/Dropbox/Scripts/mirror-tree.py %s %s" % (aptDir, os.path.join(builder.config.EVENT_DIR, 'apt')))


def write_html_page(outPath, title, content):
    f = file(outPath, 'wt')
    print >> f, "<html><head>"
    print >> f, '<meta http-equiv="Content-Type" content="text/html;charset=UTF-8">'
    print >> f, "<title>%s</title>" % title
    print >> f, "<link href='http://fonts.googleapis.com/css?family=Open+Sans:400italic,400,300,700' rel='stylesheet' type='text/css'>"
    print >> f, "<link href='http://files.dengine.net/build.css' rel='stylesheet' type='text/css'>"
    print >> f, "</head><body><div id='content-outer'><div id='content-inner'>"
    print >> f, "<h1>%s</h1>" % title
    print >> f, content
    print >> f, "</div></div></body>"
    print >> f, "</html>"


def write_report_html(tag):
    ev = builder.Event(tag)
    write_html_page(ev.file_path('index.html'), 'Build %i' % ev.number(),
                    ev.html_description(False))


def update_feed():
    """Generate events.rss into the event directory."""
    
    feedName = os.path.join(builder.config.EVENT_DIR, "events.rss")
    print "Updating feed in %s..." % feedName
    
    out = file(feedName, 'wt')
    print >> out, '<?xml version="1.0" encoding="UTF-8"?>'
    print >> out, '<rss version="2.0" xmlns:atom="http://www.w3.org/2005/Atom">'
    print >> out, '<channel>'
    
    print >> out, '<title>Doomsday Engine Builds</title>'
    print >> out, '<link>http://dengine.net/</link>'
    print >> out, '<atom:link href="%s/events.rss" rel="self" type="application/rss+xml" />' % builder.config.BUILD_URI
    print >> out, '<description>Automated binary builds of the Doomsday Engine.</description>'
    print >> out, '<language>en-us</language>'
    print >> out, '<webMaster>skyjake@users.sourceforge.net (Jaakko Keränen)</webMaster>'
    print >> out, '<lastBuildDate>%s</lastBuildDate>' % time.strftime(builder.config.RFC_TIME, 
        time.gmtime(builder.find_newest_event()['time']))
    print >> out, '<generator>autobuild.py</generator>'
    print >> out, '<ttl>180</ttl>' # 3 hours
    
    allEvents = []
    
    for timestamp, ev in builder.events_by_time():
        allEvents.append((timestamp, ev))
        print >> out, '<item>'
        print >> out, '<title>Build %i</title>' % ev.number()
        print >> out, '<link>%s/%s/</link>' % ("http://dengine.net", ev.tag())
        print >> out, '<author>skyjake@users.sourceforge.net (skyjake)</author>'
        print >> out, '<pubDate>%s</pubDate>' % time.strftime(builder.config.RFC_TIME, time.gmtime(timestamp))
        print >> out, '<atom:summary>%s</atom:summary>' % ev.text_summary()
        print >> out, '<description>%s</description>' % ev.html_description()
        print >> out, '<guid isPermaLink="false">%s</guid>' % ev.tag()
        print >> out, '</item>'
        
        write_report_html(ev.tag())
    
    # Close.
    print >> out, '</channel>'
    print >> out, '</rss>'
    
    # Write a index page for all the builds.
    versions = {}
    text = '<p class="links"><a href="events.rss">RSS Feed</a> | <a href="events.xml">XML Feed</a></p>'
    text += '<h2>Latest Builds</h2>'
    text += '<div class="buildlist">'    
    for timestamp, ev in allEvents:
        eventVersion = '.'.join(ev.version().split('.')[:2])
        if eventVersion in versions:
            versions[eventVersion].append(ev)
        else:
            versions[eventVersion] = [ev]
        text += '<div class="build %s"><a href="http://files.dengine.net/builds/build%i"><div class="buildnumber">%i</div><div class="builddate">%s</div><div class="buildversion">%s</div></a></div>' % (ev.release_type(), ev.number(), ev.number(),
            time.strftime('%b %d', time.gmtime(timestamp)), ev.version())
    text += '</div>'
    
    text += '<h2>Versions</h2>'
    for version in versions.keys():
        text += '<h3>%s</h3>' % version
        text += '<div class="buildlist">'    
        for ev in versions[version]:
            text += '<div class="build %s"><a href="http://files.dengine.net/builds/build%i"><div class="buildnumber">%i</div><div class="builddate">%s</div><div class="buildversion">%s</div></a></div>' % (ev.release_type(), ev.number(), ev.number(),
                time.strftime('%b %d', time.gmtime(ev.timestamp())), ev.version())
        text += '</div>'
            
    write_html_page(os.path.join(builder.config.EVENT_DIR, "index.html"),
                    'Doomsday Autobuilder', text)
    
    
def update_xml_feed():
    """Generate events.xml into the event directory."""
    
    feedName = os.path.join(builder.config.EVENT_DIR, "events.xml")
    print "Updating XML feed in %s..." % feedName
    
    out = file(feedName, 'wt')
    print >> out, '<?xml version="1.0" encoding="UTF-8"?>'
    print >> out, '<log>'
    for timestamp, ev in builder.events_by_time():
        print >> out, ev.xml_description()    
    print >> out, '</log>'
    

def purge_apt_repository(atLeastSeconds):
    for d in ['i386', 'amd64']:
        binDir = os.path.join(builder.config.APT_REPO_DIR, builder.config.APT_DIST + '/main/binary-') + d
        print 'Pruning binary apt directory', binDir
        # Find the old files.
        for fn in os.listdir(binDir):
            if fn[-4:] != '.deb': continue
            debPath = os.path.join(binDir, fn)
            ct = os.stat(debPath).st_ctime
            if time.time() - ct >= atLeastSeconds:
                print 'Deleting', debPath
                os.remove(debPath)


def purge_obsolete():
    """Purge old builds from the event directory (old > 12 weeks)."""
    threshold = 3600 * 24 * 7 * 12

    # Also purge the apt repository if one has been specified.
    if builder.config.APT_REPO_DIR:
        purge_apt_repository(threshold)
    
    # Purge the old events.
    print 'Deleting build events older than 12 weeks...'
    for ev in builder.find_old_events(threshold):
        print ev.tag()
        shutil.rmtree(ev.path()) 
        
    print 'Purge done.'


def dir_cleanup():
    """Purges empty build directories from the event directory."""
    print 'Event directory cleanup starting...'
    for emptyEventPath in builder.find_empty_events():
        print 'Deleting', emptyEventPath
        os.rmdir(emptyEventPath)
    print 'Cleanup done.'


def system_command(cmd):
    result = subprocess.call(cmd, shell=True)
    if result != 0:
        raise Exception("Error from " + cmd)


def generate_apidoc():
    """Run Doxygen to generate all API documentation."""
    git_pull()

    print >> sys.stderr, "\nSDK docs..."
    os.chdir(os.path.join(builder.config.DISTRIB_DIR, '../doomsday'))    
    system_command('doxygen sdk.doxy >/dev/null 2>doxyissues-sdk.txt')
    system_command('wc -l doxyissues-sdk.txt')

    print >> sys.stderr, "\nSDK docs for Qt Creator..."
    os.chdir(os.path.join(builder.config.DISTRIB_DIR, '../doomsday'))    
    system_command('doxygen sdk-qch.doxy >/dev/null 2>doxyissues-qch.txt')
    system_command('wc -l doxyissues-qch.txt')

    print >> sys.stderr, "\nPublic API docs..."
    os.chdir(os.path.join(builder.config.DISTRIB_DIR, '../doomsday/apps/libdoomsday'))    
    system_command('doxygen api.doxy >/dev/null 2>../../doxyissues-api.txt')
    system_command('wc -l ../../doxyissues-api.txt')

    print >> sys.stderr, "\nInternal Win32 docs..."
    os.chdir(os.path.join(builder.config.DISTRIB_DIR, '../doomsday/apps/client'))
    system_command('doxygen client-win32.doxy >/dev/null 2>../../doxyissues-win32.txt')
    system_command('wc -l ../../doxyissues-win32.txt')

    print >> sys.stderr, "\nInternal Mac/Unix docs..."
    system_command('doxygen client-mac.doxy >/dev/null 2>../../doxyissues-mac.txt')        
    system_command('wc -l ../../doxyissues-mac.txt')

    print >> sys.stderr, "\nDoom plugin docs..."
    os.chdir(os.path.join(builder.config.DISTRIB_DIR, '../doomsday/apps/plugins/doom'))
    system_command('doxygen doom.doxy >/dev/null 2>../../../doxyissues-doom.txt')
    system_command('wc -l ../../../doxyissues-doom.txt')

    print >> sys.stderr, "\nHeretic plugin docs..."
    os.chdir(os.path.join(builder.config.DISTRIB_DIR, '../doomsday/apps/plugins/heretic'))
    system_command('doxygen heretic.doxy >/dev/null 2>../../../doxyissues-heretic.txt')
    system_command('wc -l ../../../doxyissues-heretic.txt')

    print >> sys.stderr, "\nHexen plugin docs..."
    os.chdir(os.path.join(builder.config.DISTRIB_DIR, '../doomsday/apps/plugins/hexen'))
    system_command('doxygen hexen.doxy >/dev/null 2>../../../doxyissues-hexen.txt')
    system_command('wc -l ../../../doxyissues-hexen.txt')


def generate_wiki():
    """Automatically generate wiki pages."""
    git_pull()
    sys.path += ['/Users/jaakko/Dropbox/Scripts']
    import dew
    dew.login()
    # Today's event data.
    ev = builder.Event(latestAvailable=True)
    if ev.release_type() == 'stable':
        dew.submitPage('Latest Doomsday release',
            '#REDIRECT [[Doomsday version %s]]' % ev.version())
    dew.logout()
    

def web_path():
    return os.path.join(builder.config.DISTRIB_DIR, '..', 'web')
    
    
def web_manifest_filename():
    return os.path.join(builder.config.DISTRIB_DIR, '..', '.web.manifest')
    

def web_save(state):
    pickle.dump(state, file(web_manifest_filename(), 'wb'), pickle.HIGHEST_PROTOCOL)
    
    
def web_init():
    print 'Initializing web update manifest.'
    web_save(DirState(web_path()))
    
    
def web_update():
    print 'Checking for web file changes...'
    git_pull()
    
    oldState = pickle.load(file(web_manifest_filename(), 'rb'))
    state = DirState(web_path())
    updated = state.list_new_files(oldState)
    removed = state.list_removed(oldState)
    
    # Save the updated state.
    web_save(state)
    
    # Is there anything to do?
    if not updated and not removed[0] and not removed[1]:
        print 'Everything up-to-date.'
        return
    
    # Compile the update package.
    print 'Updated:', updated
    print 'Removed:', removed
    arcFn = os.path.join(builder.config.DISTRIB_DIR, '..', 
        'web_update_%s.zip' % time.strftime('%Y%m%d-%H%M%S'))
    arc = zipfile.ZipFile(arcFn, 'w')
    for up in updated:
        arc.write(os.path.join(web_path(), up), 'updated/' + up)
    if removed[0] or removed[1]:
        tmpFn = '_removed.tmp'
        tmp = file(tmpFn, 'wt')
        for n in removed[0]:        
            print >> tmp, 'rm', n
        for d in removed[1]:
            print >> tmp, 'rmdir', d
        tmp.close()
        arc.write(tmpFn, 'removed.txt')
        os.remove(tmpFn)
    arc.close()
    
    # Deliver the update to the website.
    print 'Delivering', arcFn
    system_command('scp %s dengine@dengine.net:www/incoming/' % arcFn)
    
    # No need to keep a local copy.
    os.remove(arcFn)
    

def show_help():
    """Prints a description of each command."""
    for cmd in sorted_commands():
        if commands[cmd].__doc__:
            print "%-17s " % (cmd + ":") + commands[cmd].__doc__
        else:
            print cmd
    

def sorted_commands():
    sc = commands.keys()
    sc.sort()
    return sc


commands = {
    'pull': pull_from_branch,
    'create': create_build_event,
    'platform_release': todays_platform_release,
    'sign': sign_packages,
    'publish': publish_packages,
    'changes': update_changes,
    'debchanges': update_debian_changelog,
    'source': build_source_package,
    'apt': rebuild_apt_repository,
    'feed': update_feed,
    'xmlfeed': update_xml_feed,
    'purge': purge_obsolete,
    'cleanup': dir_cleanup,
    'apidoc': generate_apidoc,
    'wiki': generate_wiki,
    'web_init': web_init,
    'web_update': web_update,
    'help': show_help
}

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print 'The arguments must be: (command) [args]'
        print 'Commands:', string.join(sorted_commands())
        print 'Arguments:'
        print '--branch   Branch to use (default: master)'
        print '--distrib  Doomsday distrib directory'
        print '--events   Event directory (builds are stored here in subdirs)'
        print '--apt      Apt repository'
        print '--tagmod   Additional suffix for build tag for platform_release'
        sys.exit(1)

    if sys.argv[1] not in commands:
        print 'Unknown command:', sys.argv[1]
        sys.exit(1)

    commands[sys.argv[1]]()
