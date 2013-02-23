# Copies all updated binaries to the distrib/products folder.
# Called from Visual Studio as a post-build step for all projects.

import sys, os, shutil

solDir = sys.argv[1]
buildType = sys.argv[2]


def copy_if_newer(src, dest):
    if os.path.exists(dest):
        srcTime = os.stat(src).st_mtime
        destTime = os.stat(dest).st_mtime
        if srcTime <= destTime: return    
    try:
        shutil.copyfile(src, dest)
        print 'DEPLOY :', os.path.basename(src), '->', os.path.abspath(dest)
    except IOError:
        pass


# Set up the destination.
dest = os.path.abspath(os.path.join(solDir, '..', 'distrib', 'products', 'bin'))
if not os.path.exists(dest): os.makedirs(dest)
if not os.path.exists(dest + '\\plugins'): os.makedirs(dest + '\\plugins')
if not os.path.exists(dest + '\\..\\data'): os.makedirs(dest + '\\..\\data')

# Update .pk3s.
copy_if_newer(os.path.join(solDir, 'doomsday.pk3'), os.path.join(dest, '..', 'data', 'doomsday.pk3'))
copy_if_newer(os.path.join(solDir, 'libdoom.pk3'), os.path.join(dest, '..', 'data', 'jdoom', 'libdoom.pk3'))
copy_if_newer(os.path.join(solDir, 'libheretic.pk3'), os.path.join(dest, '..', 'data', 'jheretic', 'libdoom.pk3'))
copy_if_newer(os.path.join(solDir, 'libhexen.pk3'), os.path.join(dest, '..', 'data', 'jhexen', 'libhexen.pk3'))
copy_if_newer(os.path.join(solDir, 'libdoom64.pk3'), os.path.join(dest, '..', 'data', 'jdoom64', 'libdoom64.pk3'))

targets = ['libdeng2', 'libdeng1', 'libshell', 'libgui',
           'client', 'server', 
           'plugins\*', 'tools\*', 'tools\shell\*', 'tests\*'] 

for target in targets:
    dirs = []
    if '*' in target:
        try:
            dirs += [target[:-1] + d + '\\' + buildType for d in os.listdir(os.path.join(solDir, target[:-1]))]
        except:
            pass
    else:
        dirs += [target + '\\' + buildType]
        
    for dir in dirs:
        d = os.path.join(solDir, dir)    
        if not os.path.exists(d): continue
               
        for fn in os.listdir(d):
            if fn[-4:] == '.exe':
                #print '- exe:', fn
                copy_if_newer(os.path.join(d, fn), os.path.join(dest, fn))
            elif fn[-4:] == '.dll':
                destPath = dest
                if 'plugins' in dir:
                    #print '- plugin dll:', fn
                    destPath += '\\plugins'
                #else:
                #    print '- dll:', fn
                copy_if_newer(os.path.join(d, fn), os.path.join(destPath, fn))
