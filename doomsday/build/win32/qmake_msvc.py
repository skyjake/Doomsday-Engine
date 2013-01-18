# qmake_msvc.py is a script that generates a full Visual Studio solution with 
# a .vcxproj for each subproject. You must set up envconfig.bat before running 
# the script. The solution is always placed in a folder called 
# "doomsday-msvc-build" at the repository root.
#
# qmake_msvc.py must be called whenever the .pro/.pri files change. The .sln
# and .vcxproj files are then rewritten -- manual edits will be gone!
# 
# The solution automatically deploys all built binaries into the
# "distrib\products" folder. The user is expected to 1) copy the required
# dependency .dlls to products\bin, and 2) define the appropriate launch
# command line and options (which are persistently then stored in the 
# .vcxproj.user file).

import os, sys, re

print "\nVISUAL STUDIO SOLUTION GENERATOR FOR DOOMSDAY\n"
print "Author: <jaakko.keranen@iki.fi>\n"

# Figure out the Doomsday root path.
rootPath = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]), '..', '..'))
solPath = os.path.abspath(os.path.join(rootPath, '..', 'doomsday-msvc-build'))

print "Doomsday root path:", rootPath
print "Solution location: ", solPath

if not os.path.exists(solPath): os.mkdir(solPath)

# Run qmake to generate the solution and vcprojs.
os.chdir(solPath)
#os.system(os.path.join(rootPath, 'build', 'win32', 'qmake_tp_vc.bat'))

# Cleanup.
print "\nDeleting spurious PK3s..."
for name in ['doomsday', 'libdoom', 'libheretic', 'libhexen', 'libdoom64']:
    fn = "..\%s.pk3" % name
    if os.path.exists(fn):
        os.remove(fn)

print "\nPatching..."

def fileRegex(fn, pattern, repl, count=0, startAtPos=0):
    text = file(fn, 'r').read()
    skipped = text[:startAtPos]
    text = text[startAtPos:]
    text = re.sub(pattern, repl, text, count)
    file(fn, 'w').write(skipped + text)
    
def fileRegOff(fn, pattern, startAtPos=0):
    text = file(fn, 'r').read()
    text = text[startAtPos:]
    found = re.search(pattern, text)
    return found.start()
    

fileRegex('libdeng2\deng2.vcxproj', 
          r'=\\&quot;([A-Za-z0-9_\- ]+)\\&quot;', 
          r'=&quot;\1&quot;')
    
copyScript = 'python &quot;%s\\\\build\win32\copy_to_products.py' % rootPath + '&quot; &quot;' + solPath + '&quot;'
rcDirs = '&quot;' + rootPath + '\\\\api&quot;;&quot;' + rootPath + '\\\\libdeng1\include' + '&quot;'

def find_projs(path):
    found = []
    for name in os.listdir(path):
        fn = os.path.join(path, name)
        if os.path.isdir(fn) and '.' not in name:
            found += find_projs(fn)
        elif name[-8:] == '.vcxproj':
            found.append(os.path.join(path, name))
    return found
 
allProjs = find_projs(solPath)

print "Found %i projects." % len(allProjs)

for fn in allProjs:
    pName = os.path.basename(fn)
    pName = pName[:pName.index('.')]
    
    # Fix target names.
    fileRegex(fn, '[0-2]</PrimaryOutput>', '</PrimaryOutput>')
    fileRegex(fn, '[0-2]</TargetName>', '</TargetName>') 

    # Add a post-build step for Debug config.
    start = fileRegOff(fn, r'<ItemDefinitionGroup Condition.*Debug\|Win32')
    fileRegex(fn, 
              r'</ResourceCompile>\n',
              r"""</ResourceCompile>
    <PostBuildEvent>
      <Command>""" + copyScript + r""" Debug</Command>
    </PostBuildEvent>
    <PostBuildEvent>
      <Message>Copy """ + pName + r""" to products/bin</Message>
    </PostBuildEvent>\n""", 1, start)
    
    # Tell RC where to find headers.
    fileRegex(fn,
              r'</ResourceCompile>\n',
              r"""  <AdditionalIncludeDirectories>""" + rcDirs + """</AdditionalIncludeDirectories>\n    </ResourceCompile>\n""", 
              1, start)
   
    # Add a post-build step for Release config.
    start = fileRegOff(fn, r'<ItemDefinitionGroup Condition.*Release\|Win32')

    fileRegex(fn, 
              r'</ResourceCompile>\n',
              r"""</ResourceCompile>
    <PostBuildEvent>
      <Command>""" + copyScript + r""" Release</Command>
    </PostBuildEvent>
    <PostBuildEvent>
      <Message>Copy """ + pName + r""" to products/bin</Message>
    </PostBuildEvent>\n""", 1, start)
    
    # Tell RC where to find headers.
    fileRegex(fn,
              r'</ResourceCompile>\n',
              r"""  <AdditionalIncludeDirectories>""" + rcDirs + """</AdditionalIncludeDirectories>\n    </ResourceCompile>\n""", 
              1, start)    
    
print "\nAll done!\n"
print 'Solution generated:', os.path.join(solPath, 'doomsday.sln')
