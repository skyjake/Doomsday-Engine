# $Id$
# Build script for Mac OS X
# Now uses py2app instead of BundleBuilder

from distutils.core import setup
import py2app

ver = file('VERSION').read().strip()

name = 'Doomsday Engine'
iconfile = 'graphics/snowberry.icns'
verstr = 'deng ' + ver

includes = []

resources = []

plist = dict(
    CFBundleIconFile           = iconfile,
    CFBundleName               = name,
    CFBundleShortVersionString = ver,
    CFBundleGetInfoString      = verstr,
    CFBundleExecutable         = name,
    CFBundleIdentifier         = 'net.sourceforge.deng'
)

opts = dict(py2app = dict(
    compressed = 1,
    iconfile = iconfile,
    plist = plist,
    includes = includes,
    optimize = 2
    ))
    
setup( app = ['snowberry.py'], data_files = resources, options = opts )




#import bundlebuilder, os
#
#root = "/Users/jaakko/Projects/Snowberry"

# Create the AppBuilder
#app = bundlebuilder.AppBuilder(verbosity=1)

#app.mainprogram = os.path.join(root, "snowberry.py")

#app.standalone = 1
#app.name = "Doomsday Engine"
#app.iconfile = 'graphics/snowberry.icns'

# includePackages forces inclusion of packages
#app.includePackages.append("something")

# Include resources into the package.
#app.resources.append(os.path.join(root, "build/addons"))
#app.resources.append(os.path.join(root, "build/graphics"))
#app.resources.append(os.path.join(root, "build/conf"))
#app.resources.append(os.path.join(root, "build/lang"))
#app.resources.append(os.path.join(root, "build/profiles"))
#app.resources.append(os.path.join(root, "build/plugins"))

# Include the wxWidgets shared libraries.
#app.libs.append("/usr/local/lib/wxPython-unicode-2.6.1.0/lib/libwx_macud-2.6.0.dylib")
#app.libs.append("/usr/local/lib/wxPython-unicode-2.6.1.0/lib/libwx_macud-2.6.0.rsrc")

# Build the app.
#app.setup()
#app.build()
