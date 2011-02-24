#!/usr/bin/python
import os, sys, shutil, time, glob

LAUNCH_DIR = os.getcwd()
DOOMSDAY_DIR = os.path.join(os.getcwd(), '..', 'doomsday')
SNOWBERRY_DIR = os.path.join(LAUNCH_DIR, '..', 'snowberry')
WORK_DIR = os.path.join(LAUNCH_DIR, 'work')
OUTPUT_DIR = os.path.join(os.getcwd(), 'releases')
DOOMSDAY_VERSION = "0.0.0-Name"
DOOMSDAY_VERSION_PLAIN = "0.0.0"
TIMESTAMP = time.strftime('%y-%m-%d')


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
   
    
def find_version():
    print "Determining Doomsday version number...",
    
    versionBase = None
    versionName = None
    
    f = file(os.path.join(DOOMSDAY_DIR, "engine", "portable", "include", "dd_version.h"), 'rt')
    for line in f.readlines():
        line = line.strip()
        if line[:7] != "#define": continue
        baseAt = line.find("DOOMSDAY_VERSION_BASE")
        nameAt = line.find("DOOMSDAY_RELEASE_NAME")
        if baseAt > 0:
            versionBase = line[baseAt + 21:].replace('\"','').strip()
        if nameAt > 0:
            versionName = line[nameAt + 21:].replace('\"','').strip()

    global DOOMSDAY_VERSION
    global DOOMSDAY_VERSION_PLAIN
    DOOMSDAY_VERSION_PLAIN = versionBase
    DOOMSDAY_VERSION = versionBase
    if versionName:
        DOOMSDAY_VERSION += "-" + versionName    
        
    print DOOMSDAY_VERSION


def prepare_work_dir():
    remkdir(WORK_DIR)    
    print "Work directory prepared."
    

"""The Mac OS X release procedure."""
def mac_release():
    # First we need to make a release build.
    print "Building the release..."
    os.chdir(WORK_DIR)
    mkdir('release_build')
    os.chdir('release_build')
    if os.system('cmake ' + DOOMSDAY_DIR + ' && make'):
        raise Exception("Failed to build from source.")
        
    # Now we can proceed to packaging.
    target = OUTPUT_DIR + "/deng-" + DOOMSDAY_VERSION + "_" + TIMESTAMP + ".dmg"
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
    
    # Back to the work dir.
    os.chdir(WORK_DIR)
    copytree(SNOWBERRY_DIR + '/dist/Doomsday Engine.app', 'Doomsday Engine.app')
    
    print 'Coping release binaries into the launcher bundle.'
    copytree('release_build/Doomsday.app', 'Doomsday Engine.app/Contents/Doomsday.app')
    for f in glob.glob('release_build/*.bundle'):
        copytree(f, 'Doomsday Engine.app/Contents/' + os.path.basename(f))
        
    print 'Creating disk image:', target
    
    masterDmg = target
    volumeName = "Doomsday Engine " + DOOMSDAY_VERSION
    shutil.copy(SNOWBERRY_DIR + '/template-image/template.dmg', 'imaging.dmg')
    remkdir('imaging')
    os.system('hdiutil attach imaging.dmg -noautoopen -quiet -mountpoint imaging')
    shutil.rmtree('imaging/Doomsday Engine.app', True)
    remove('imaging/Read Me.rtf')
    copytree('Doomsday Engine.app', 'imaging/Doomsday Engine.app')
    shutil.copy(DOOMSDAY_DIR + "/build/mac/Read Me.rtf", 'imaging/Read Me.rtf')
    
    os.system('diskutil rename ' + os.path.abspath('imaging') + 
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
        .replace('${VERSION}', DOOMSDAY_VERSION)
        .replace('${VERSION_PLAIN}', DOOMSDAY_VERSION_PLAIN))
    
    # Execute the win32 release script.
    os.chdir('win32')
    if os.system('dorel.bat'):
        raise Exception("Failure in the Windows release script.")
    

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
