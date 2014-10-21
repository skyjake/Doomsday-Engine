#!/usr/bin/env python

import os, re, subprocess

rootDir = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '../..'))

def list_projects():
    return subprocess.check_output(['find', '.', '-name', 'project.pbxproj']).split()

print("Generating Xcode projects, hang on...")
for i in range(2):
    os.system("qmake-qt5 -r -spec macx-xcode %s/doomsday.pro CONFIG+='debug deng_notests deng_noccache' > /dev/null 2> /dev/null" % rootDir)
    count = len(list_projects())
    print 'Got %i project files.' % count
    # qmake is quite broken when generating Xcode projects; we'll run again for more success.
    if count > 50: break    
    if i == 0: print "Second time's the charm? Generating again..."

print('Built products will be symlinked in distrib/products/')
for fn in list_projects():
    #print 'patching', fn
    content = file(fn, 'r').read()
    # Symlink products.
    content = re.sub(r'cp -r \$BUILT_PRODUCTS_DIR',
                      'ln -fs $BUILT_PRODUCTS_DIR', content)
    # Quote library paths with space (-L).
    def space_repl(parens):
        def rep(path): return '"\\"' + path.group(1) + '\\""'
        return parens.group(1) + re.sub(r'"([^"]* [^"]*)"', rep, parens.group(3)) + parens.group(4)
    content = re.sub(r'((LIBRARY_SEARCH_PATHS|OTHER_LDFLAGS) = \()([^\)]*)(\);)', space_repl, content)    
    file(fn, 'w').write(content)

print('Setting up a workspace...')
workName = 'Doomsday.xcworkspace'
os.system('mkdir -p %s && cp %s/build/mac/contents.xcworkspacedata %s/' % (workName, rootDir, workName))

print('All done! Open %s in Xcode.' % workName)
