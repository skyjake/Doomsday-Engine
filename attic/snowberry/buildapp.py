# $Id$
# Build script for Mac OS X
# Now uses py2app instead of BundleBuilder

from distutils.core import setup
import py2app

ver = file('VERSION').read().strip()

name = 'Doomsday Engine'
iconfile = 'snowberry.icns'
verstr = ver + '-macosx'

includes = ['webbrowser']

resources = [
    'build/addons',
    'build/conf',
    'build/graphics',
    'build/lang',
    'build/plugins',
    'build/profiles'
]

plist = dict(
    CFBundleIconFile           = iconfile,
    CFBundleName               = name,
    CFBundleShortVersionString = ver,
    CFBundleGetInfoString      = verstr,
    CFBundleExecutable         = name,
    CFBundleIdentifier         = 'net.dengine.snowberry'
)

opts = dict(py2app = dict(
    compressed = 1,
    iconfile = iconfile,
    plist = plist,
    includes = includes,
    optimize = 2
    ))

setup( app = ['snowberry.py'], data_files = resources, options = opts )
