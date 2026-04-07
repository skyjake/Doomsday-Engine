/** @file resourceclass.cpp
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "doomsday/resourceclass.h"
#include "doomsday/filesys/filetype.h"

using namespace de;
using namespace res;

static ResourceClass &(*classGetter)(resourceclassid_t) = 0;

DE_PIMPL_NOREF(ResourceClass)
{
    /// Symbolic name for this class.
    String name;

    /// Symbolic name of the default filesystem subspace scheme.
    String defaultScheme;

    /// Recognized file types (in order of importance, left to right; owned).
    FileTypes fileTypes;

    ~Impl()
    {
        deleteAll(fileTypes);
    }
};

ResourceClass::ResourceClass(String name, String defaultScheme)
    : d(new Impl)
{
    d->name = name;
    d->defaultScheme = defaultScheme;
}

String ResourceClass::name() const
{
    return d->name;
}

String ResourceClass::defaultScheme() const
{
    return d->defaultScheme;
}

int ResourceClass::fileTypeCount() const
{
    return d->fileTypes.size();
}

ResourceClass &ResourceClass::addFileType(FileType *ftype)
{
    d->fileTypes.append(ftype);
    return *this;
}

const ResourceClass::FileTypes &ResourceClass::fileTypes() const
{
    return d->fileTypes;
}

bool ResourceClass::isNull() const
{
    static const String nullName = "RC_NULL";
    return d->name == nullName;
}

ResourceClass &ResourceClass::classForId(resourceclassid_t id)
{
    DE_ASSERT(classGetter != 0);
    return classGetter(id);
}

void ResourceClass::setResourceClassCallback(ResourceClass &(*callback)(resourceclassid_t))
{
    classGetter = callback;
}
