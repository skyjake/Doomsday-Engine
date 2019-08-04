#!/usr/bin/python
#
# This script will generate the debian/ directory required for creating
# a source package. Binary packages are created by CPack, so they do
# not need this.

import os, subprocess, sys

DENG_ROOT = os.getcwd()
OUT_DIR = os.path.join(DENG_ROOT, 'debian')
TEMPLATE_DIR = os.path.join(DENG_ROOT, 'doomsday/build/debian/template')

if not os.path.exists(OUT_DIR):
    os.mkdir(OUT_DIR)

ARCH = subprocess.check_output(['dpkg', '--print-architecture']).strip()
DEBFULLNAME = os.getenv('DEBFULLNAME')
DEBEMAIL = os.getenv('DEBEMAIL')
PACKAGE = 'doomsday'
BUILDNUMBER = sys.argv[1]

control = open(os.path.join(TEMPLATE_DIR, 'control'), 'r').read()
control = control.replace('${Package}', PACKAGE)
control = control.replace('${Arch}', ARCH)
control = control.replace('${DEBFULLNAME}', DEBFULLNAME)
control = control.replace('${DEBEMAIL}', DEBEMAIL)
open(os.path.join(OUT_DIR, 'control'), 'w').write(control)

rules = open(os.path.join(TEMPLATE_DIR, 'rules'), 'r').read()
rules = rules.replace('${BuildNumber}', BUILDNUMBER)
open(os.path.join(OUT_DIR, 'rules'), 'w').write(rules)

compat = open(os.path.join(TEMPLATE_DIR, 'compat'), 'r').read()
open(os.path.join(OUT_DIR, 'compat'), 'w').write(compat)

copyright = open(os.path.join(TEMPLATE_DIR, 'copyright'), 'r').read()
open(os.path.join(OUT_DIR, 'copyright'), 'w').write(copyright)

docs = open(os.path.join(TEMPLATE_DIR, 'docs'), 'r').read()
open(os.path.join(OUT_DIR, 'docs'), 'w').write(docs)

