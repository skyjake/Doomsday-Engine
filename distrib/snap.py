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
# -all			Include ALL definitions files
# -gfx			Include Data\Graphics\
# -sdl			Include SDL libraries
# -test			Don't upload

import os, tempfile, sys, shutil, re

baseDir = 'D:/Projects/de1.8/doomsday'
outDir  = 'D:/Projects/de1.8/distrib/Out'
binDir  = 'Bin/Debug' # or 'Release'

# Create the temporary directory hierarchy.
archDir = tempfile.mktemp()
print "Using temporary directory: " + archDir
os.mkdir( archDir )
neededDirs = ['Bin']
if '-def' in sys.argv:
	neededDirs += ['Defs', 'Defs\jDoom', 'Defs\jHeretic', 'Defs\jHexen']
if '-gfx' in sys.argv:
	neededDirs += ['Data', 'Data\Graphics']
for sub in neededDirs:
	os.mkdir( os.path.join( archDir, sub ) )

files = [ (binDir + '/Doomsday.exe', 'Bin/Doomsday.exe'),
          (binDir + '/jDoom.dll', 'Bin/jDoom.dll')
          #,
          #(binDir + '/jHeretic.dll', 'Bin/jHeretic.dll'),
          #(binDir + '/jHexen.dll', 'Bin/jHexen.dll') 
          ]

# Select the appropriate files.
if not '-nodll' in sys.argv:
	files.append( (binDir + '/drOpenGL.dll', 'Bin/drOpenGL.dll') )
	files.append( (binDir + '/dpDehRead.dll', 'Bin/dpDehRead.dll') )
	if not '-nosnd' in sys.argv:
		files += [ (binDir + '/dsA3D.dll', 'Bin/dsA3D.dll'),
		           (binDir + '/dsCompat.dll', 'Bin/dsCompat.dll') ]
	if not '-ogl' in sys.argv:
		files.append( (binDir + '/drD3D.dll', 'Bin/drD3D.dll') )

# These defs will be included, if present.
includeDefs = ['Anim.ded', 'Finales.ded', 'jDoom.ded', 'jHeretic.ded',
               'jHexen.ded', 'TNTFinales.ded', 'PlutFinales.ded']

excludeDefs = ['Objects.ded', 'Players.ded', 'HUDWeapons.ded', 'Weapons.ded',
               'Items.ded', 'Monsters.ded', 'Technology.ded', 'Nature.ded',
               'Decorations.ded', 'FX.ded', 'Models.ded',
               'TextureParticles.ded', 'Details.ded', 'Doom1Lights.ded',
               'Doom2Lights.ded']

if '-all' in sys.argv:
	allDefs = True
else:
	allDefs = False

if '-def' in sys.argv:
	for game in ['.', 'jDoom', 'jHeretic', 'jHexen']:
		gameDir = os.path.join( 'Defs', game )
		defDir = os.path.join( baseDir, gameDir )
		for file in os.listdir( defDir ):
			# Should this be included?
			if file in includeDefs or (allDefs and not file in excludeDefs \
			and re.match( "(?i).*\.ded$", file )):
				loc = os.path.join( gameDir, file )
				files.append( (loc, loc) )
				
# Graphics?
if '-gfx' in sys.argv:
	files.append( ('Data\Doomsday.wad', 'Data\Doomsday.wad') )
	for file in os.listdir( os.path.join( baseDir, 'Data/Graphics' ) ):
		loc = os.path.join( 'Data/Graphics', file )
		files.append( (loc, loc) )
		
# SDL?
if '-sdl' in sys.argv:
	for file in os.listdir( os.path.join( baseDir, 'DLLs' ) ):
		if re.match( "(?i)^sdl.*\.dll$", file ):
			files.append( (os.path.join( 'DLLs', file ), 
						   os.path.join( 'Bin', file )) )

# Copy the files.
for src, dest in files:
	try:
		shutil.copyfile( os.path.join( baseDir, src ),
		                 os.path.join( archDir, dest ) )
	except IOError, (errno, strerror):
		print "I/O error(%s): %s" % (errno, strerror)
		print "  Skipping " + os.path.join( baseDir, src ) + "..."

# Make the archive.
outFile = os.path.join( outDir, 'deng-snap-1.8.alpha-1.exe' )
try:
	os.remove( outFile )
except:
	pass
os.system( "rar -sfx -r -ep1 -m5 a %s %s\\*" % (outFile, archDir) )

# Generate Self-Extracting RAR comment.
commentFile = os.path.join( archDir, '_comment.txt' )
f = open( commentFile, 'w' )
f.write( "Title=Doomsday Engine Binary Snapshot\n" )
f.write( "Overwrite=1\n" )
f.write( "Text\n{\n" )
for line in open( 'RelNotes.html' ).readlines():
	f.write( line )
f.write( "\n}\n" )
f.close()

# Add comment to the the EXE.
os.system( "rar c -z%s %s" % (commentFile, outFile) )

print "Removing temporary directory: " + archDir
shutil.rmtree( archDir )

"""
if not '-test' in sys.argv:
	# Upload.
	print "Uploading to the Mirror..."
	os.system( "ftpscrpt -f z:\scripts\mirror-snapshot.scp" )
	print "Uploading to Fourwinds..."
	os.system( "ftpscrpt -f z:\scripts\snapshot.scp" )
else:
	print "--- THIS WAS JUST A TEST ---"
"""	

print "Done!"
