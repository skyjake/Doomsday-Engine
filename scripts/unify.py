import os, sys

comps = ['libdoom', 'libheretic', 'libhexen']

def unify(mode):
    for fn in os.listdir('libdoom/'+mode):
        # Do the others have this?
        content = file('libdoom/%s/%s' % (mode, fn)).read()

        try:
            if content == file('libheretic/%s/%s' % (mode, fn)).read() and \
               content == file('libhexen/%s/%s' % (mode, fn)).read():
           
               file('libcommon/%s/%s' % (mode, fn), 'w').write(content)
               for com in comps:
                   os.remove('%s/%s/%s' % (com, mode, fn))        
               print 'unified', mode, fn
        except IOError:
            pass
        
unify('variable')
unify('command')
