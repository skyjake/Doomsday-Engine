/** @file doomsdaypackage.cpp
 *
 * @authors Copyright (c) 2016 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "doomsday/res/DoomsdayPackage"

#include <de/NativeFile>

using namespace de;

namespace res {

static String const PACKAGE_DEFS_PATH("package.defsPath");

DoomsdayPackage::DoomsdayPackage(Package const &package)
    : _pkg(package)
{}

File const &DoomsdayPackage::sourceFile() const
{
    return _pkg.sourceFile();
}

bool DoomsdayPackage::hasDefinitions() const
{
    return _pkg.objectNamespace().has(PACKAGE_DEFS_PATH);
}

String DoomsdayPackage::defsPath() const
{
    return _pkg.objectNamespace().gets(PACKAGE_DEFS_PATH, "");
}

Uri DoomsdayPackage::loadableUri() const
{
    return loadableUri(_pkg.file());
}

bool DoomsdayPackage::hasDefinitions(File const &packageFile) // static
{
    return packageFile.objectNamespace().has(PACKAGE_DEFS_PATH);
}

String DoomsdayPackage::defsPath(File const &packageFile) // static
{
    return packageFile.objectNamespace().gets(PACKAGE_DEFS_PATH, "");
}

Uri DoomsdayPackage::loadableUri(File const &packageFile) // static
{
    if (NativeFile const *nativeSrc = packageFile.source()->maybeAs<NativeFile>())
    {
        return Uri::fromNativePath(nativeSrc->nativePath());
    }
    return Uri();
}

} // namespace res
