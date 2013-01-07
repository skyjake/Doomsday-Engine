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

#ifndef LIBDENG_RESOURCECLASS_H
#define LIBDENG_RESOURCECLASS_H

#include "api_resourceclass.h"

#ifdef __cplusplus
#ifndef DENG2_C_API_ONLY

#include <QList>
#include <de/String>
#include "filetype.h"

namespace de
{
    /**
     * ResourceClass encapsulates the properties and logics belonging to a logical
     * class of resource (e.g., Graphic, Model, Sound, etc...)
     *
     * @ingroup base
     */
    struct ResourceClass
    {
    public:
        typedef QList<FileType*> FileTypes;

    public:
        ResourceClass(String _name, String _defaultScheme)
            : name_(_name), defaultScheme_(_defaultScheme)
        {}

        virtual ~ResourceClass() {
            DENG2_FOR_EACH(FileTypes, i, fileTypes_)
            {
                delete *i;
            }
        }

        /// Return the symbolic name of this resource class.
        String const& name() const
        {
            return name_;
        }

        /// Return the symbolic name of the default filesystem subspace scheme
        /// for this class of resource.
        String const& defaultScheme() const
        {
            return defaultScheme_;
        }

        /// Return the number of file types for this class of resource.
        int fileTypeCount() const
        {
            return fileTypes_.count();
        }

        /**
         * Add a new file type to the resource class. ResourceClass takes ownership.
         * Earlier types have priority.
         *
         * @param ftype  File type to add.
         * @return  This instance.
         */
        ResourceClass& addFileType(FileType& ftype)
        {
            fileTypes_.push_back(&ftype);
            return *this;
        }

        /**
         * Provides access to the file type list for efficient iteration.
         *
         * @return  List of file types for this class of resource.
         */
        FileTypes const& fileTypes() const
        {
            return fileTypes_;
        }

    private:
        /// Symbolic name for this class.
        String name_;

        /// Symbolic name of the default filesystem subspace scheme.
        String defaultScheme_;

        /// Recognized file types (in order of importance, left to right).
        FileTypes fileTypes_;
    };

    /**
     * The special "null" ResourceClass object.
     *
     * @ingroup core
     */
    struct NullResourceClass : public ResourceClass
    {
        NullResourceClass() : ResourceClass("FC_NONE",  "")
        {}
    };

    /// @return  @c true= @a rclass is a "null-resourceclass" object (not a real class).
    inline bool isNullResourceClass(ResourceClass const& rclass) {
        return !!dynamic_cast<NullResourceClass const*>(&rclass);
    }

} // namespace de
#endif // DENG2_C_API_ONLY
#endif // __cplusplus

#endif /* LIBDENG_RESOURCECLASS_H */
