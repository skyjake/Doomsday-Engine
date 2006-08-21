#!/usr/bin/python
# This Python script will create a set of PK3 files that contain the files
# that Doomsday needs at runtime. The PK3 files are organized using the 
# traditional data/ and defs/ structure.

import time, sys, os, os.path, zipfile

if len(sys.argv) < 2:
    print "Usage: %s pk3-target-dir" % sys.argv[0]
    print "Run from build/scripts/."
    sys.exit(0)

deng_dir = '../..'
target_dir = sys.argv[1]

class Pack:
    def __init__(self):
        self.files = [] # tuples
        
    def add_files(self, fileNamesArray):
        self.files += fileNamesArray
        
    def create(self, name):
        full_name = os.path.join(target_dir, name)
        print "creating %s as %s" % (name, full_name)
        
        pk3 = zipfile.ZipFile(full_name, 'w', zipfile.ZIP_DEFLATED)
        
        for src, dest in self.files:
            full_src = os.path.join(deng_dir, src)
            # Is this a file or a folder?
            if os.path.isfile(full_src):
                # Write the file as is.
                print "writing %s as %s" % (full_src, dest)
                pk3.write(full_src, dest)
            elif os.path.isdir(full_src):
                # Write the contents of the folder recursively.
                def process_dir(path, dest_path):
                    print "processing", path
                    for file in os.listdir(path):
                        real_file = os.path.join(path, file)
                        if file[0] == '.':
                            continue # Ignore these.
                        if os.path.isfile(real_file):
                            print "writing %s as %s" % (real_file, os.path.join(dest_path, file))
                            pk3.write(real_file, os.path.join(dest_path, file))
                        elif os.path.isdir(real_file):
                            process_dir(real_file, 
                                        os.path.join(dest_path, file))
                process_dir(full_src, dest)
            
        # Write it out.
        print "closing", full_name
        pk3.close()

# First up, doomsday.pk3.
# Directory contents added recursively.
p = Pack()
p.add_files(
    [ ('engine/defs', 'defs'),
      ('engine/data', 'data') ] )
p.create('doomsday.pk3')

# jdoom.pk3
p = Pack()
p.add_files(
    [ ('plugins/jdoom/defs', 'defs/jdoom'),
      ('plugins/jdoom/data', 'data/jdoom') ] )
p.create('jdoom.pk3')

# jheretic.pk3
p = Pack()
p.add_files(
    [ ('plugins/jheretic/defs', 'defs/jheretic'),
      ('plugins/jheretic/data', 'data/jheretic') ] )
p.create('jheretic.pk3')

# jhexen.pk3
p = Pack()
p.add_files(
    [ ('plugins/jhexen/defs', 'defs/jhexen'),
      ('plugins/jhexen/data', 'data/jhexen') ] )
p.create('jhexen.pk3')
