#!/usr/bin/python
# This script builds the distribution packages platform-independently.
# No parameters needed; config is auto-detected.

import sys
import os
import platform
import shutil
import time
import glob
import build_version
import build_number

# Configuration.
LAUNCH_DIR    = os.path.abspath(os.getcwd())
DOOMSDAY_DIR  = os.path.abspath(os.path.join(os.getcwd(), '..', 'doomsday'))
SNOWBERRY_DIR = os.path.abspath(os.path.join(LAUNCH_DIR, '..', 'snowberry'))
WORK_DIR      = os.path.join(LAUNCH_DIR, 'work')
OUTPUT_DIR    = os.path.abspath(os.path.join(os.getcwd(), 'releases'))
DOOMSDAY_VERSION_FULL       = "0.0.0-Name"
DOOMSDAY_VERSION_FULL_PLAIN = "0.0.0"
DOOMSDAY_VERSION_MAJOR      = 0
DOOMSDAY_VERSION_MINOR      = 0
DOOMSDAY_VERSION_REVISION   = 0
DOOMSDAY_RELEASE_TYPE       = "Unstable"
DOOMSDAY_BUILD_NUMBER       = build_number.todays_build()
DOOMSDAY_BUILD              = 'build' + DOOMSDAY_BUILD_NUMBER

TIMESTAMP = time.strftime('%y-%m-%d')
now = time.localtime()


def exit_with_error():
    os.chdir(LAUNCH_DIR)
    sys.exit(1)


def mkdir(n):
    try:
        os.mkdir(n)
    except OSError:
        print 'Directory', n, 'already exists.'


def remkdir(n):
    if os.path.exists(n):
        print n, 'exists, clearing it...'
        shutil.rmtree(n, True)
    os.mkdir(n)


def remove(n):
    try:
        os.remove(n)
    except OSError:
        print 'Cannot remove', n


def copytree(s, d):
    try:
        shutil.copytree(s, d)
    except Exception, x:
        print x
        print 'Cannot copy', s, 'to', d


def duptree(s, d):
    os.system('cp -fRp "%s" "%s"' % (s, d))


def find_version():
    build_version.find_version()

    global DOOMSDAY_VERSION_FULL
    global DOOMSDAY_VERSION_FULL_PLAIN
    global DOOMSDAY_VERSION_MAJOR
    global DOOMSDAY_VERSION_MINOR
    global DOOMSDAY_VERSION_REVISION
    global DOOMSDAY_RELEASE_TYPE

    DOOMSDAY_RELEASE_TYPE = build_version.DOOMSDAY_RELEASE_TYPE
    DOOMSDAY_VERSION_FULL_PLAIN = build_version.DOOMSDAY_VERSION_FULL_PLAIN
    DOOMSDAY_VERSION_FULL = build_version.DOOMSDAY_VERSION_FULL
    DOOMSDAY_VERSION_MAJOR = build_version.DOOMSDAY_VERSION_MAJOR
    DOOMSDAY_VERSION_MINOR = build_version.DOOMSDAY_VERSION_MINOR
    DOOMSDAY_VERSION_REVISION = build_version.DOOMSDAY_VERSION_REVISION
    
    print 'Build:', DOOMSDAY_BUILD, 'on', TIMESTAMP
    print 'Version:', DOOMSDAY_VERSION_FULL_PLAIN, DOOMSDAY_RELEASE_TYPE


def prepare_work_dir():
    remkdir(WORK_DIR)
    print "Work directory prepared."


def mac_os_version():
    """Determines the Mac OS version."""
    return platform.mac_ver()[0][:4]


def mac_target_ext():
    if mac_os_version() == '10.8': return '.dmg'
    if mac_os_version() == '10.6': return '_mac10_6.dmg'
    return '_32bit.dmg'


def output_filename(ext=''):
    if DOOMSDAY_RELEASE_TYPE == "Stable":
        return 'doomsday_' + DOOMSDAY_VERSION_FULL + ext
    else:
        return 'doomsday_' + DOOMSDAY_VERSION_FULL + "_" + DOOMSDAY_BUILD + ext


def mac_able_to_package_snowberry():
    return mac_os_version() == '10.5'
    

def mac_package_snowberry():
    os.chdir(SNOWBERRY_DIR)
    remkdir('dist')
    remkdir('build')

    print 'Copying resources...'
    remkdir('build/addons')
    remkdir('build/conf')
    remkdir('build/graphics')
    remkdir('build/lang')
    remkdir('build/profiles')
    remkdir('build/plugins')

    for f in ['/conf/osx-appearance.conf',
              '/conf/osx-components.conf',
              '/conf/osx-doomsday.conf',
              '/conf/snowberry.conf']:
        shutil.copy(SNOWBERRY_DIR + f, 'build/conf')

    for f in (glob.glob(SNOWBERRY_DIR + '/graphics/*.jpg') +
              glob.glob(SNOWBERRY_DIR + '/graphics/*.png') +
              glob.glob(SNOWBERRY_DIR + '/graphics/*.bmp') +
              glob.glob(SNOWBERRY_DIR + '/graphics/*.ico')):
        shutil.copy(f, 'build/graphics')

    for f in glob.glob(SNOWBERRY_DIR + '/lang/*.lang'):
        shutil.copy(f, 'build/lang')

    for f in glob.glob(SNOWBERRY_DIR + '/profiles/*.prof'):
        shutil.copy(f, 'build/profiles')

    for f in glob.glob(SNOWBERRY_DIR + '/plugins/tab*.py'):
        shutil.copy(f, 'build/plugins')

    for f in ['tab30.plugin', 'about.py', 'help.py', 'launcher.py',
              'preferences.py', 'profilelist.py', 'wizard.py']:
        src = SNOWBERRY_DIR + '/plugins/' + f
        if os.path.isdir(src):
            copytree(src, 'build/plugins/' + f)
        else:
            shutil.copy(src, 'build/plugins')

    f = file('VERSION', 'wt')
    f.write(DOOMSDAY_VERSION_FULL)
    f.close()
    os.system('python buildapp.py py2app')
    
    # Share it.
    duptree('dist/Doomsday Engine.app', 'shared/')


def mac_release():
    """The Mac OS X release procedure."""

    # Package Snowberry or acquire the shared package.
    if mac_able_to_package_snowberry():
        # Check Python dependencies.
        try:
            import wx
        except ImportError:
            raise Exception("Python: wx not found!")
        try:
            import py2app
        except ImportError:
            raise Exception("Python: py2app not found!")

        mac_package_snowberry()
        sbLoc = '/dist/Doomsday Engine.app'
    else:
        # Wait until the updated packaged SB has been shared.
        try:
            print 'This system seems unable to package Snowberry. Waiting a while'
            print 'for an updated shared Doomsday Engine.app bundle...'
            print '(press Ctrl-C to skip)'
            time.sleep(5 * 60)
        except KeyboardInterrupt:
            pass
        sbLoc = '/shared/Doomsday Engine.app'

    # First we need to make a release build.
    print "Building the release..."
    # Must work in the deng root for qmake (resource bundling apparently
    # fails otherwise).
    MAC_WORK_DIR = os.path.abspath(os.path.join(DOOMSDAY_DIR, '../macx_release_build'))
    remkdir(MAC_WORK_DIR)
    os.chdir(MAC_WORK_DIR)
    if os.system('qmake -r -spec macx-g++ CONFIG+=release DENG_BUILD=%s ' % (DOOMSDAY_BUILD_NUMBER) +
                 '../doomsday/doomsday.pro && make -j2 -w'):
        raise Exception("Failed to build from source.")

    # Now we can proceed to packaging.
    target = os.path.join(OUTPUT_DIR, output_filename(mac_target_ext()))
    try:
        os.remove(target)
        print 'Removed existing target file', target
    except:
        print 'Target:', target

    # Back to the normal work dir.
    os.chdir(WORK_DIR)
    copytree(SNOWBERRY_DIR + sbLoc, 'Doomsday Engine.app')

    print 'Copying release binaries into the launcher bundle.'
    duptree(os.path.join(MAC_WORK_DIR, 'client/Doomsday.app'), 'Doomsday Engine.app/Contents/Doomsday.app')
    for f in glob.glob(os.path.join(MAC_WORK_DIR, 'client/*.bundle')):
        # Exclude jDoom64.
        if not 'jDoom64' in f:
            duptree(f, 'Doomsday Engine.app/Contents/' + os.path.basename(f))
    duptree(os.path.join(MAC_WORK_DIR, 'tools/shell/shell-gui/Doomsday Shell.app'), 'Doomsday Shell.app')

    print 'Correcting permissions...'
    os.system('chmod -R o-w "Doomsday Engine.app"')
    os.system('chmod -R o-w "Doomsday Shell.app"')

    print 'Signing Doomsday.app...'
    os.system('codesign --verbose -s "Developer ID Application: Jaakko Keranen" "Doomsday Engine.app/Contents/Doomsday.app"')

    print 'Signing Doomsday Engine.app...'
    os.system('codesign --verbose -s "Developer ID Application: Jaakko Keranen" "Doomsday Engine.app"')

    print 'Signing Doomsday Shell.app...'
    os.system('codesign --verbose -s "Developer ID Application: Jaakko Keranen" "Doomsday Shell.app"')

    print 'Creating disk:', target
    os.system('osascript /Users/jaakko/Dropbox/Doomsday/package-installer.applescript')
    
    masterPkg = target
    volumeName = "Doomsday Engine " + DOOMSDAY_VERSION_FULL
    templateFile = os.path.join(SNOWBERRY_DIR, 'template-image/template.sparseimage')
    if not os.path.exists(templateFile):
        print 'Template .sparseimage not found, trying to extract from compressed archive...'
        os.system('bunzip2 -k "%s.bz2"' % templateFile)
    shutil.copy(templateFile, 'imaging.sparseimage')
    remkdir('imaging')
    os.system('hdiutil attach imaging.sparseimage -noautoopen -quiet -mountpoint imaging')
    shutil.copy('/Users/jaakko/Desktop/Doomsday.pkg', 'imaging/Doomsday.pkg')
    shutil.copy(os.path.join(DOOMSDAY_DIR, "doc/output/Read Me.rtf"), 'imaging/Read Me.rtf')

    volumeName = "Doomsday Engine " + DOOMSDAY_VERSION_FULL
    os.system('/usr/sbin/diskutil rename ' + os.path.abspath('imaging') + ' "' + volumeName + '"')

    os.system('hdiutil detach -quiet imaging')
    os.system('hdiutil convert imaging.sparseimage -format UDZO -imagekey zlib-level=9 -o "' + target + '"')
    remove('imaging.sparseimage')


def win_release():
    """The Windows release procedure."""
    
    PROD_DIR = os.path.join(LAUNCH_DIR, 'products')
    if not os.path.exists(PROD_DIR):
        print "Creating the products directory."
        os.mkdir(PROD_DIR)

    PROD_DATA_DIR = os.path.join(PROD_DIR, 'data')
    if not os.path.exists(PROD_DATA_DIR):
        print "Creating the products/data directory."
        os.mkdir(PROD_DATA_DIR)

    PROD_DOC_DIR = os.path.join(PROD_DIR, 'doc')
    if not os.path.exists(PROD_DOC_DIR):
        print "Creating the products/doc directory."
        os.mkdir(PROD_DOC_DIR)

    # Generate the Inno Setup configuration file.
    script = file('win32\setup.iss.template', 'rt').read()
    file('win32\setup.iss', 'wt').write(script
        .replace('${YEAR}', time.strftime('%Y'))
        .replace('${BUILD}', DOOMSDAY_BUILD)
        .replace('${VERSION}', DOOMSDAY_VERSION_FULL)
        .replace('${VERSION_PLAIN}', DOOMSDAY_VERSION_FULL_PLAIN)
        .replace('${OUTPUT_FILENAME}', output_filename()))

    # Execute the win32 release script.
    os.chdir('win32')
    if os.system('dorel.bat ' + DOOMSDAY_BUILD_NUMBER):
        raise Exception("Failure in the Windows release script.")


def linux_release():
    """The Linux release procedure."""
    
    os.chdir(LAUNCH_DIR)

    def clean_products():
        # Remove previously built deb packages.
        os.system('rm -f ../doomsday*.deb ../doomsday*.changes ../doomsday*.tar.gz ../doomsday*.dsc')
        os.system('rm -f doomsday-fmod*.deb doomsday-fmod*.changes doomsday-fmod*.tar.gz doomsday-fmod*.dsc')
        #os.system('rm -f dsfmod/fmod-*.txt')

    clean_products()

    # Check that the changelog exists.
    if not os.path.exists('debian/changelog'):
        os.system('dch --check-dirname-level=0 --create --package doomsday -v %s-%s "Initial release."' % (DOOMSDAY_VERSION, DOOMSDAY_BUILD))

    if os.system('linux/gencontrol.sh && dpkg-buildpackage -b'):
        raise Exception("Failure to build from source.")

    # Build dsFMOD separately.
    os.chdir('dsfmod')
    logSuffix = "%s-%s.txt" % (sys.platform, platform.architecture()[0])
    if os.system('LD_LIBRARY_PATH=`pwd`/../builddir/libdeng2:`pwd`/../builddir/libdeng dpkg-buildpackage -b > fmod-out-%s 2> fmod-err-%s' % (logSuffix, logSuffix)):
        raise Exception("Failure to build dsFMOD from source.")
    shutil.copy(glob.glob('../doomsday-fmod*.deb')[0], OUTPUT_DIR)
    shutil.copy(glob.glob('../doomsday-fmod*.changes')[0], OUTPUT_DIR)    
    os.chdir('..')

    # Place the result in the output directory.
    shutil.copy(glob.glob('../doomsday*.deb')[0], OUTPUT_DIR)
    shutil.copy(glob.glob('../doomsday*.changes')[0], OUTPUT_DIR)
    clean_products()


def main():
    prepare_work_dir()
    find_version()

    print "Checking OS...",

    try:
        if sys.platform == "darwin":
            print "Mac OS X (%s)" % mac_os_version()
            mac_release()
        elif sys.platform == "win32":
            print "Windows"
            win_release()
        elif sys.platform == "linux2":
            print "Linux"
            linux_release()
        else:
            print "Unknown!"
            print "I don't know how to make a release on this platform."
            exit_with_error()
    except Exception, x:
        print "Creating the release failed!"
        print x
        exit_with_error()

    os.chdir(LAUNCH_DIR)
    print "Done."


if __name__ == '__main__':
    main()
