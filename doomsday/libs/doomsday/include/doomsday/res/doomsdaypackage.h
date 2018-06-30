/** @file doomsdaypackage.h
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

#ifndef LIBDOOMSDAY_DOOMSDAYPACKAGE_H
#define LIBDOOMSDAY_DOOMSDAYPACKAGE_H

#include <de/Package>
#include "../uri.h"

namespace res {

/**
 * Utility for accessing Doomsday-specific content in packages. Only keeps a reference
 * to a de::Package and accesses its contents.
 *
 * The static methods can be used to access the contents of unloaded package files.
 */
class LIBDOOMSDAY_PUBLIC DoomsdayPackage
{
public:
    DoomsdayPackage(de::Package const &package);

    de::File const &sourceFile() const;

    bool hasDefinitions() const;

    de::String defsPath() const;

    res::Uri loadableUri() const;

public:
    static bool hasDefinitions(de::File const &packageFile);

    static de::String defsPath(de::File const &packageFile);

    /**
     * Returns the URI of the package for loading via FS1.
     */
    static res::Uri loadableUri(de::File const &packageFile);

private:
    de::Package const &_pkg;
};

} // namespace res

#endif // LIBDOOMSDAY_DOOMSDAYPACKAGE_H
