# Doomsday Project Snapshot Builder
# $Id$
#
# Command line options specify which files to include.
# Files will be copied to a temporary directory, from which
# a RAR archive is created.
#
# Options:
# -nodll		Exclude non-game DLLs
# -nosnd		Exclude sound DLLs
# -ogl			Exclude Direct3D
# -def			Include definition files

import os, tempfile, sys, shutil

baseDir = 'C:/Projects/deng/doomsday'
outDir  = 'C:/Projects/deng/distrib/out'

# Create the temporary directory.
archDir = tempfile.mktemp()
print "Using temporary directory: " + archDir
os.mkdir( archDir )
neededDirs = ['Bin']
if '-def' in sys.argv:
	neededDirs += ['Defs', 'Defs\jDoom', 'Defs\jHeretic', 'Defs\jHexen']
for sub in neededDirs:
	os.mkdir( os.path.join( archDir, sub ) )

files = [ ('Bin/Release/Doomsday.exe', 'Bin/Doomsday.exe'),
          ('Bin/Release/jDoom.dll', 'Bin/jDoom.dll'),
          ('Bin/Release/jHeretic.dll', 'Bin/jHeretic.dll'),
          ('Bin/Release/jHexen.dll', 'Bin/jHexen.dll') ]

# Select the appropriate files.
if not '-nodll' in sys.argv:
	files.append( ('Bin/Release/drOpenGL.dll', 'Bin/drOpenGL.dll') )
	files.append( ('Bin/Release/dpDehRead.dll', 'Bin/dpDehRead.dll') )
	if not '-nosnd' in sys.argv:
		files += [ ('Bin/Release/dsA3D.dll', 'Bin/dsA3D.dll'),
		           ('Bin/Release/dsCompat.dll', 'Bin/dsCompat.dll') ]
	if not '-ogl' in sys.argv:
		files.append( ('Bin/Release/drD3D.dll', 'Bin/drD3D.dll') )

# These defs will be included, if present.
includeDefs = ['Anim.ded', 'Finales.ded', 'jDoom.ded', 'jHeretic.ded',
               'jHexen.ded', 'TNTFinales.ded', 'PlutFinales.ded']

if '-def' in sys.argv:
	for game in ['jDoom', 'jHeretic', 'jHexen']:
		gameDir = 'Defs\\' + game
		defDir = os.path.join( baseDir, gameDir )
		for file in os.listdir( defDir ):
			# Should this be included?
			if file in includeDefs: # and re.match( "(?i).*\.ded$", file ):
				loc = os.path.join( gameDir, file )
				files.append( (loc, loc) )

# Copy the files.
for src, dest in files:
	shutil.copyfile( os.path.join( baseDir, src ),
	                 os.path.join( archDir, dest ) )

# Make the archive.
outFile = os.path.join( outDir, 'ddsnapshot.rar' )
os.remove( outFile )
os.system( "rar -r -ep1 -m5 a %s %s\\*" % (outFile, archDir) )

print "Removing temporary directory: " + archDir
shutil.rmtree( archDir )

# Upload.
print "Uploading to the Mirror..."
os.system( "ftpscrpt -f z:\scripts\mirror-snapshot.scp" )

print "Uploading to Fourwinds..."
os.system( "ftpscrpt -f z:\scripts\snapshot.scp" )

print "Done!"
