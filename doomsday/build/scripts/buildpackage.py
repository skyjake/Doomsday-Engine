#!/usr/bin/env python2.7
#
# Command line utility for zipping a directory of files into a .pack.
# Checks that the required Info file is present.  
#
# Usage:
#   buildpackage (pack-dir) (output-dir)
#

import sys, os, os.path, zipfile

if len(sys.argv) < 2:
    print "Usage: %s (pack-dir) (output-dir)" % sys.argv[0]
    sys.exit(0)

# Check quiet flag.
class Package:
    def __init__(self, sourcePath):
        self.sourcePath = sourcePath
        
    def build(self, outputPath):
        # Ensure the output path exists.
        try:
            os.makedirs(outputPath)
        except:
            pass
    
        outputName = os.path.join(outputPath, os.path.basename(self.sourcePath))
        pack = zipfile.ZipFile(outputName, 'w', zipfile.ZIP_DEFLATED)        
        contents = []
            
        # Index the contents of the folder recursively.
        def descend(path):
            for name in os.listdir(os.path.join(self.sourcePath, path)):
                if name[0] == '.':
                    continue # Ignore these.

                if len(path):
                    internalPath = os.path.join(path, name)
                else:
                    internalPath = name
                fullPath = os.path.join(self.sourcePath, internalPath)
                    
                if os.path.isfile(fullPath):
                    contents.append((fullPath, internalPath))
                elif os.path.isdir(fullPath):
                    descend(internalPath)
        descend('')

        # Check for the required metadata file.
        foundInfo = False
        for full, internal in contents:
            if internal.lower() == 'info' or internal.lower() == 'info.dei':
                foundInfo = True
                break            
        if not foundInfo:
            print "No 'Info' file found in \"%s\"!" % self.sourcePath
            sys.exit(1)
        
        # Write entries in alphabetical order.
        for full, internal in sorted(contents):
            pack.write(full, internal)
            
        # Write it out.
        print "Wrote %s (contains %i files)." % (outputName, len(pack.namelist()))
        pack.close()

if __name__ == "__main__":
    p = Package(sys.argv[1])
    p.build(sys.argv[2])
