#!/usr/bin/python
import sys
import os
import platform
import shutil
import time
import glob
import build_version

LAUNCH_DIR = os.getcwd()
DOOMSDAY_DIR = os.path.join(os.getcwd(), '..', 'doomsday')
SNOWBERRY_DIR = os.path.join(LAUNCH_DIR, '..', 'snowberry')
WORK_DIR = os.path.join(LAUNCH_DIR, 'work')
OUTPUT_DIR = os.path.join(os.getcwd(), 'releases')
DOOMSDAY_VERSION = "0.0.0-Name"
DOOMSDAY_VERSION_PLAIN = "0.0.0"
DOOMSDAY_RELEASE_TYPE = "Unstable"
now = time.localtime()
DOOMSDAY_BUILD_NUMBER = str((now.tm_year - 2011)*365 + now.tm_yday)
DOOMSDAY_BUILD = 'build' + DOOMSDAY_BUILD_NUMBER
TIMESTAMP = time.strftime('%y-%m-%d')

print 'Build:', DOOMSDAY_BUILD, 'on', TIMESTAMP


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
    
    global DOOMSDAY_VERSION
    global DOOMSDAY_VERSION_PLAIN
    global DOOMSDAY_RELEASE_TYPE
    
    DOOMSDAY_RELEASE_TYPE = build_version.DOOMSDAY_RELEASE_TYPE
    DOOMSDAY_VERSION_PLAIN = build_version.DOOMSDAY_VERSION_PLAIN
    DOOMSDAY_VERSION = build_version.DOOMSDAY_VERSION


def prepare_work_dir():
    remkdir(WORK_DIR)    
    print "Work directory prepared."


def mac_os_version():
    return platform.mac_ver()[0][:4]
    
    
    

"""The Mac OS X release procedure."""
def mac_release():
    # Check Python dependencies.
    try:
        import wx
    except ImportError:
        raise Exception("Python: wx not found!")
    try:
        import py2app
    except ImportError:
        raise Exception("Python: py2app not found!")
    # First we need to make a release build.
    print "Building the release..."
    # Must work in the deng root for qmake (resource bundling apparently 
    # fails otherwise).
    MAC_WORK_DIR = os.path.join(DOOMSDAY_DIR, '../macx_release_build')
    remkdir(MAC_WORK_DIR)
    os.chdir(MAC_WORK_DIR)
    if os.system('qmake -r -spec macx-g++ CONFIG+=release DENG_BUILD=%s ' % (DOOMSDAY_BUILD_NUMBER) + 
                 '../doomsday/doomsday.pro && make -w ' + 
                 '&& ../doomsday/build/mac/bundleapp.sh ../doomsday'):
        raise Exception("Failed to build from source.")
        
    # Now we can proceed to packaging.
    target = OUTPUT_DIR + "/doomsday_" + DOOMSDAY_VERSION + "_" + DOOMSDAY_BUILD + ".dmg"
    try:
        os.remove(target)
        print 'Removed existing target file', target
    except:
        print 'Target:', target
    
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
    f.write(DOOMSDAY_VERSION)
    f.close()
    os.system('python buildapp.py py2app')
    
    # Back to the normal work dir.
    os.chdir(WORK_DIR)
    copytree(SNOWBERRY_DIR + '/dist/Doomsday Engine.app', 'Doomsday Engine.app')
    
    print 'Coping release binaries into the launcher bundle.'
    duptree(os.path.join(MAC_WORK_DIR, 'engine/Doomsday.app'), 'Doomsday Engine.app/Contents/Doomsday.app')
    for f in glob.glob(os.path.join(MAC_WORK_DIR, 'engine/*.bundle')):
        # Exclude jDoom64.
        if not 'jDoom64' in f:
            duptree(f, 'Doomsday Engine.app/Contents/' + os.path.basename(f))
        
    print 'Creating disk image:', target
    
    masterDmg = target
    volumeName = "Doomsday Engine " + DOOMSDAY_VERSION
    templateFile = os.path.join(SNOWBERRY_DIR, 'template-image/template.dmg')
    if not os.path.exists(templateFile):
        print 'Template .dmg not found, trying to extract from compressed archive...'
        os.system('bunzip2 -k "%s.bz2"' % templateFile)
    shutil.copy(templateFile, 'imaging.dmg')
    remkdir('imaging')
    os.system('hdiutil attach imaging.dmg -noautoopen -quiet -mountpoint imaging')
    shutil.rmtree('imaging/Doomsday Engine.app', True)
    remove('imaging/Read Me.rtf')
    duptree('Doomsday Engine.app', 'imaging/Doomsday Engine.app')
    shutil.copy(LAUNCH_DIR + "/mac/Read Me.rtf", 'imaging/Read Me.rtf')
    
    os.system('/usr/sbin/diskutil rename ' + os.path.abspath('imaging') + 
        ' "' + "Doomsday Engine " + DOOMSDAY_VERSION + '"')
    
    os.system('hdiutil detach -quiet imaging')
    os.system('hdiutil convert imaging.dmg -format UDZO -imagekey zlib-level=9 -o "' + target + '"')
    remove('imaging.dmg')


"""The Mac OS X release procedure."""
def win_release():
    # Generate the Inno Setup configuration file.
    script = file('win32\setup.iss.template', 'rt').read()
    file('win32\setup.iss', 'wt').write(script
        .replace('${YEAR}', time.strftime('%Y'))
        .replace('${BUILD}', DOOMSDAY_BUILD)
        .replace('${VERSION}', DOOMSDAY_VERSION)
        .replace('${VERSION_PLAIN}', DOOMSDAY_VERSION_PLAIN))
    
    # Execute the win32 release script.
    os.chdir('win32')
    if os.system('dorel.bat ' + DOOMSDAY_BUILD_NUMBER):
        raise Exception("Failure in the Windows release script.")
        
        
"""The Linux release procedure."""
def linux_release():
    os.chdir(LAUNCH_DIR)
    
    # Generate a launcher script.
    f = file('linux/launch-doomsday', 'wt')
    print >> f, """#!/usr/bin/python
import os, sys
os.chdir('/usr/share/doomsday/snowberry')
sys.path += '.'

import snowberry"""
    f.close()
    
    def clean_products():
        # Remove previously build deb packages.
        os.system('rm -f ../doomsday*.deb ../doomsday*.changes ../doomsday*.tar.gz ../doomsday*.dsc')
        
    clean_products()
       
    if os.system('linux/gencontrol.sh && dpkg-buildpackage -b'):
        raise Exception("Failure to build from source.")
        
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
            print "Mac OS X"
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
