#!/usr/bin/python
# This Python script will create a set of PK3 files that contain the files
# that Doomsday needs at runtime. The PK3 files are organized using the 
# traditional data/ and defs/ structure.

import sys, os, os.path, zipfile

if len(sys.argv) < 2:
    print "Usage: %s pk3-target-dir" % sys.argv[0]
    print "(run in build/scripts/)"
    sys.exit(0)

deng_dir = os.path.join('..', '..')
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
    [ ('libdeng/defs', 'defs'),
      ('libdeng/data', 'data') ] )
p.create('doomsday.pk3')

# doom.pk3
p = Pack()
p.add_files(
    [ ('plugins/doom/defs', 'defs/jdoom'),
      ('plugins/doom/data/conhelp.txt', 'data/doom/conhelp.txt'),
      ('plugins/doom/data/lumps', '#.basedata') ] )
p.create('doom.pk3')

# heretic.pk3
p = Pack()
p.add_files(
    [ ('plugins/heretic/defs', 'defs/jheretic'),
      ('plugins/heretic/data/conhelp.txt', 'data/heretic/conhelp.txt'),
      ('plugins/heretic/data/lumps', '#.basedata') ] )
p.create('heretic.pk3')

# hexen.pk3
p = Pack()
p.add_files(
    [ ('plugins/hexen/defs', 'defs/jhexen'),
      ('plugins/hexen/data/conhelp.txt', 'data/hexen/conhelp.txt'),
      ('plugins/hexen/data/lumps', '#.basedata') ] )
p.create('hexen.pk3')

# jdoom64.pk3
p = Pack()
p.add_files(
    [ ('plugins/jdoom64/defs', 'defs/jdoom64'),
      ('plugins/jdoom64/data/conhelp.txt', 'data/jdoom64/conhelp.txt'),
      ('plugins/jdoom64/data/lumps', '#.basedata') ] )
p.create('jdoom64.pk3')
