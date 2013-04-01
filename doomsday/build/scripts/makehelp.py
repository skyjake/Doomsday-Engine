#!/usr/bin/python

import os, sys
import conhelp

dengRoot = os.path.abspath(os.path.join(
    os.path.dirname(os.path.abspath(sys.argv[0])), '..', '..'))
    
conHelp = os.path.join(dengRoot, 'build', 'scripts', 'conhelp.py')

os.chdir(os.path.join(dengRoot, 'doc'))

print "Engine help"
cpHelpTxt = os.path.join(dengRoot, 'client', 'data', 'cphelp.txt')
conhelp.makeHelp(cpHelpTxt, ['engine'])

panel = conhelp.amethyst(file(os.path.join('engine', 'controlpanel.ame')).read())

file(cpHelpTxt, 'a').write(panel)

print "libdoom help"
conhelp.makeHelp(
    os.path.join(dengRoot, 'plugins', 'doom', 'data', 'conhelp.txt'),
    ['libcommon', 'libdoom'])
    
print "libheretic help"
conhelp.makeHelp(
    os.path.join(dengRoot, 'plugins', 'heretic', 'data', 'conhelp.txt'),
    ['libcommon', 'libheretic'])
    
print "libhexen help"
conhelp.makeHelp(
    os.path.join(dengRoot, 'plugins', 'hexen', 'data', 'conhelp.txt'),
    ['libcommon', 'libhexen'])
