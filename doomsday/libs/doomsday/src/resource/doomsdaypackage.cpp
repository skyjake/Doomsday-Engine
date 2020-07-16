/** @file doomsdaypackage.cpp
 *
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "doomsday/res/doomsdaypackage.h"

#include <de/nativefile.h>

using namespace de;

namespace res {

static String const PACKAGE_DEFS_PATH("package.defsPath");

DoomsdayPackage::DoomsdayPackage(const Package &package)
    : _pkg(package)
{}

const File &DoomsdayPackage::sourceFile() const
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

res::Uri DoomsdayPackage::loadableUri() const
{
    return loadableUri(_pkg.file());
}

bool DoomsdayPackage::hasDefinitions(const File &packageFile) // static
{
    return packageFile.objectNamespace().has(PACKAGE_DEFS_PATH);
}

String DoomsdayPackage::defsPath(const File &packageFile) // static
{
    return packageFile.objectNamespace().gets(PACKAGE_DEFS_PATH, "");
}

res::Uri DoomsdayPackage::loadableUri(const File &packageFile) // static
{
    if (const auto *nativeSrc = maybeAs<NativeFile>(packageFile.source()))
    {
        return res::Uri::fromNativePath(nativeSrc->nativePath());
    }
    return res::Uri();
}

} // namespace res
