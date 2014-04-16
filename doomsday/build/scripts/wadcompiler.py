#!/usr/bin/env python2.7

import sys, os.path, struct


def split_name(name):
    if '@' in name:
        pos = name.index('@')
        return (name[:pos], name[pos+1:])
    else:
        return (name, name)


def valid_lump_name(name):
    n = name.upper()
    if len(n) > 8:
        # Truncate.
        n = n[:8]
    elif len(n) < 8:
        # Pad with zeroes.
        n += chr(0) * (8 - len(n))
    return n


class Lump:
    def __init__(self, name, data, offset):
        if chr(0) in name:
            name = name[:name.index(chr(0))]
        self.name = name
        self.data = data
        self.offset = offset
        
    def nice_name(self):
        return self.name.strip(chr(0))


class Wad:
    def __init__(self, file_name, mode):
        self.file_name = file_name
        self.mode = mode
        self.file = file(file_name, mode)
        self.type = 'PWAD'
        self.lumps = []
        if 'r' in mode:
            # lumps is a dictionary.
            self.read()
        
    def read(self):
        self.type = self.file.read(4)
        count = struct.unpack('<I', self.file.read(4))[0]
        info_offset = struct.unpack('<I', self.file.read(4))[0]
        print "%s, type=%s, count=%i, info_offset=%i" % (self.file_name, self.type, count, info_offset)
        self.lumps = []
        # Read the info table.
        self.file.seek(info_offset)
        info_table = []
        for i in range(count):
            pos, size = struct.unpack('<II', self.file.read(8))
            name = self.file.read(8)
            info_table.append((pos, size, name))
        # Read the data.
        for info in info_table:
            if info[1] != 0:
                self.file.seek(info[0])
                data = self.file.read(info[1])
            else:
                data = ''
            self.lumps.append( Lump(info[2], data, info[0]) )
        
    def write(self):
        print 'writing', self.file_name
        self.file.truncate()
        self.file.seek(0)
        # Type.
        self.file.write(self.type)
        # Number of lumps and offset.
        self.file.write(struct.pack('<II', len(self.lumps), 0))
        info_table = []
        # Write each lump data.
        for lump in self.lumps:
            size = len(lump.data)
            pos = self.file.tell()
            if size == 0: pos = 0
            info_table.append((pos, size, lump.name))
            if len(lump.data) != 0:
                self.file.write(lump.data)
        
        # Mark the info table offset.
        info_offset = self.file.tell()
        
        # Write the table.
        for info in info_table:
            self.file.write(struct.pack('<II', info[0], info[1]) +
                            valid_lump_name(info[2]))
        
        # Back to write the table offset.
        self.file.seek(8)
        self.file.write(struct.pack('<I', info_offset))
               
    def close(self):
        if 'w' in self.mode or '+' in self.mode:
            self.write()
        self.file.close()
        
    def set_type(self, new_type):
        if len(new_type) != 4:
            print "'%s' is not a valid type. Must have 4 chars." % new_type
        else:
            print 'setting type to %s' % new_type
        self.type = new_type
        
    def list(self):
        for i in range(len(self.lumps)):
            lump = self.lumps[i]
            print "%5i -- %-8s (at %8i, %8i bytes)" % (i, lump.name, lump.offset, 
                                                       len(lump.data))
        print "%i lumps total." % len(self.lumps)
        
    def insert(self, at_lump, names):
        # Where to insert?
        if at_lump is None:
            at = len(self.lumps)
        else:
            at = -1
            for i in range(len(self.lumps)):
                lump = self.lumps[i]
                if lump.nice_name() == at_lump:
                    at = i
                    break
        if at == -1:
            print "could not find insert point, aborting"
            return
        print "inserting at index %i..." % at
        
        for name in names:
            real, arch = split_name(name)
            print "inserting %s as %s" % (real, valid_lump_name(arch))
            self.lumps.insert(at, Lump(valid_lump_name(arch), 
                                       file(real, 'rb').read(), 0))
    
    def extract(self, names):
        split_names = [split_name(n) for n in names]
        names = [arch.upper() for arch, real in split_names]
        for lump in self.lumps:
            nice_name = lump.nice_name()
            if len(names) == 0 or nice_name.upper() in names:
                # Check for a substitution first.
                ext = '.lmp'
                for arch, real in split_names:
                    if arch.upper() == nice_name.upper():
                        if nice_name != real:
                            nice_name = real
                            ext = ''
                        break
                # Write out this lump.
                nice_name = nice_name.lower()
                if os.path.exists(nice_name + ext):
                    counter = 1
                    while True:
                        try_name = nice_name + ext + "_" + str(counter)
                        counter += 1
                        if not os.path.exists(try_name):
                            nice_name = try_name
                            break
                else:
                    nice_name += ext
                print "extracting", lump.name, "(%i bytes) as %s" % (len(lump.data), nice_name)
                f = file(nice_name, 'wb')
                f.write(lump.data)
                f.close()


def usage():
    print 'Usage: %s [command] [wad] [files]' % sys.argv[0]
    print 'command = l(ist) | c(reate PWAD) | C(reate IWAD) | x(tract) | a(ppend) | i(nsert) | t(ype)'
    print 'Examples:'
    print '%s l some.wad' % sys.argv[0]
    print '%s c new.wad LUMP1 LUMP2 LUMP3' % sys.argv[0]
    print '%s i LUMP2 some.wad LUMP5 LUMP6 LUMP7 (at LUMP2)' % sys.argv[0]
    print '%s a existing.wad LUMP4 LUMP5' % sys.argv[0]
    print '%s x existing.wad (extracts all lumps)' % sys.argv[0]
    print '%s x existing.wad LUMP4 (extracts selected lumps)' % sys.argv[0]
    print '%s t existing.wad IWAD' % sys.argv[0]
    print 'You can specify files as realfilename@LUMPNAME.'
   
# Call main function.
print 'WAD Compiler by skyjake@users.sourceforge.net'
print '$Date$'

# Check the args.
if len(sys.argv) < 3:
    usage()
    sys.exit(0)

# Check the command.
command = sys.argv[1][0]
if command not in 'tlcCxai':
    usage()
    sys.exit(1)
     
if command == 'i':
    wad_name = sys.argv[3]
    at_lump = sys.argv[2]
    names = sys.argv[4:]
else:
    wad_name = sys.argv[2]
    names = sys.argv[3:]

# Create a new WAD file.
if command == 'c' or command == 'C':
    wad = Wad(wad_name, 'wb')
    if command == 'C': wad.set_type('IWAD')
    wad.insert(None, names)

else:
    # Open an existing file.
    mode = 'rb'
    if command == 'i' or command == 't': mode = 'rb+'
    try:
        wad = Wad(wad_name, mode)
    except:
        print 'Failure to open %s.' % wad_name
        import traceback
        traceback.print_exc()
        sys.exit(2)
    
    if command == 'l':
        wad.list()
    elif command == 't':
        wad.set_type(names[0])
    elif command == 'i':
        wad.insert(at_lump, names)
    elif command == 'a':
        wad.insert(None, names)
    elif command == 'x':
        wad.extract(names)
    
wad.close()
