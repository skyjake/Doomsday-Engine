/** @file staticlibraryfeed.cpp
 *
 * @authors Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/filesys/staticlibraryfeed.h"
#include "de/Folder"
#include "de/Library"
#include "de/LibraryFile"
#include "de/String"

namespace de {

StaticLibraryFeed::StaticLibraryFeed()
{}

String StaticLibraryFeed::description() const
{
    return "imported static libraries";
}

Feed::PopulatedFiles StaticLibraryFeed::populate(Folder const &folder)
{
    PopulatedFiles files;
#if defined (DE_STATIC_LINK)
    for (String name : Library::staticLibraries())
    {
        if (!folder.has(name))
        {
            files << new LibraryFile(NativePath(name));
        }
    }
#else
    DE_UNUSED(folder);
#endif
    return files;
}

bool StaticLibraryFeed::prune(File &) const
{
    // Static libraries are built-in, so nothing will change...
    return false;
}

} // namespace de
