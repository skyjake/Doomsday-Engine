# $Id$
# Build script for Mac OS X

import bundlebuilder, os

root = "/Users/jaakko/Projects/Snowberry"

# Create the AppBuilder
app = bundlebuilder.AppBuilder(verbosity=1)

app.mainprogram = os.path.join(root, "snowberry.py")

app.standalone = 1
app.name = "Doomsday Engine"
app.iconfile = 'graphics/snowberry.icns'

# includePackages forces inclusion of packages
#app.includePackages.append("something")

# Include resources into the package.
app.resources.append(os.path.join(root, "build/addons"))
app.resources.append(os.path.join(root, "build/graphics"))
app.resources.append(os.path.join(root, "build/conf"))
app.resources.append(os.path.join(root, "build/lang"))
app.resources.append(os.path.join(root, "build/profiles"))
app.resources.append(os.path.join(root, "build/plugins"))

# Include the wxWidgets shared libraries.
app.libs.append("/usr/local/lib/wxPython-ansi-2.5.3.1/lib/libwx_macd-2.5.3.dylib")
app.libs.append("/usr/local/lib/wxPython-ansi-2.5.3.1/lib/libwx_macd-2.5.3.rsrc")

# Build the app.
app.setup()
app.build()
