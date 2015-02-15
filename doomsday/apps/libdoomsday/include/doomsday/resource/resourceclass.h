/**
 * @file resourceclass.h
 *
 * Resource Class. @ingroup resource
 *
 * @author Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef LIBDOOMSDAY_RESOURCECLASS_H
#define LIBDOOMSDAY_RESOURCECLASS_H

#include "api_resourceclass.h"

#ifdef __cplusplus

#include "../filesys/filetype.h"
#include <de/String>
#include <QList>

/**
 * ResourceClass encapsulates the properties and logics belonging to a logical
 * class of resource (e.g., Graphic, Model, Sound, etc...)
 *
 * @ingroup base
 */
class LIBDOOMSDAY_PUBLIC ResourceClass
{
public:
    typedef QList<de::FileType *> FileTypes;

public:
    ResourceClass(de::String name, de::String defaultScheme);

    /// Return the symbolic name of this resource class.
    de::String name() const;

    /// Return the symbolic name of the default filesystem subspace scheme
    /// for this class of resource.
    de::String defaultScheme() const;

    /// Return the number of file types for this class of resource.
    int fileTypeCount() const;

    /**
     * Add a new file type to the resource class. Earlier types have priority.
     *
     * @param ftype  File type to add. ResourceClass takes ownership.
     *
     * @return This instance.
     */
    ResourceClass& addFileType(de::FileType *ftype);

    /**
     * Provides access to the file type list for efficient iteration.
     *
     * @return  List of file types for this class of resource.
     */
    FileTypes const& fileTypes() const;

    bool isNull() const;

public:
    static ResourceClass &classForId(resourceclassid_t id);

    /// @todo This is unnecessary once the resource subsystem is part of libdoomsday. -jk
    static void setResourceClassCallback(ResourceClass &(*callback)(resourceclassid_t));

private:
    DENG2_PRIVATE(d)
};

/**
 * The special "null" ResourceClass object.
 *
 * @ingroup core
 */
class LIBDOOMSDAY_PUBLIC NullResourceClass : public ResourceClass
{
public:
    NullResourceClass() : ResourceClass("RC_NULL",  "") {}
};

/// @return  @c true= @a rclass is a "null-resourceclass" object (not a real class).
inline bool isNullResourceClass(ResourceClass const& rclass) {
    return rclass.isNull();
}

#endif // __cplusplus

#endif /* LIBDOOMSDAY_RESOURCECLASS_H */
