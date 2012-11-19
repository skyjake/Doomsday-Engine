/**
 * @file fileclass.h
 *
 * File Class. @ingroup fs
 *
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_FILECLASS_H
#define LIBDENG_FILECLASS_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * File Class Identifier.
 *
 * @ingroup base
 *
 * @todo Refactor away. These identifiers are no longer needed.
 */
typedef enum fileclassid_e {
    FC_NONE = -2,           ///< Not a real class.
    FC_UNKNOWN = -1,        ///< Attempt to guess the class through evaluation of the path.
    FILECLASS_FIRST = 0,
    FC_PACKAGE = FILECLASS_FIRST,
    FC_DEFINITION,
    FC_GRAPHIC,
    FC_MODEL,
    FC_SOUND,
    FC_MUSIC,
    FC_FONT,
    FILECLASS_COUNT
} fileclassid_t;

#define VALID_FILECLASSID(n)   ((n) >= FILECLASS_FIRST && (n) < FILECLASS_COUNT)

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus
#ifndef DENG2_C_API_ONLY

#include <QList>
#include <de/String>
#include "filetype.h"

namespace de
{
    /**
     * FileClass encapsulates the properties and logics belonging to a logical
     * class of resource file (e.g., Graphic, Model, Sound, etc...)
     *
     * @ingroup base
     */
    struct FileClass
    {
    public:
        typedef QList<FileType*> Types;

    public:
        FileClass(String _name, String _defaultNamespace)
            : name_(_name), defaultNamespace_(_defaultNamespace)
        {}

        virtual ~FileClass() {};

        /// Return the symbolic name of this file class.
        String const& name() const
        {
            return name_;
        }

        /// Return the symbolic name of the default namespace for this class of file.
        String const& defaultNamespace() const
        {
            return defaultNamespace_;
        }

        /// Return the number of file types for this class.
        int fileTypeCount() const
        {
            return searchTypeOrder.count();
        }

        /**
         * Add a new type of file to this class. Earlier types have priority.
         *
         * @param ftype  File type to add.
         * @return  This instance.
         */
        FileClass& addFileType(FileType* ftype)
        {
            searchTypeOrder.push_back(ftype);
            return *this;
        }

        /**
         * Provides access to the file type list for efficient iteration.
         *
         * @return  List of file types of this class.
         */
        Types const& fileTypes() const
        {
            return searchTypeOrder;
        }

    private:
        /// Symbolic name for this class.
        String name_;

        /// Symbolic name of the default namespace.
        String defaultNamespace_;

        /// Recognized file types (in order of importance, left to right).
        Types searchTypeOrder;
    };

    /**
     * The special "null" FileClass object.
     *
     * @ingroup core
     */
    struct NullFileClass : public FileClass
    {
        NullFileClass() : FileClass("FC_NONE",  "")
        {}
    };

    /// @return  @c true= @a fclass is a "null-fileclass" object (not a real class).
    inline bool isNullFileClass(FileClass const& fclass) {
        return !!dynamic_cast<NullFileClass const*>(&fclass);
    }

} // namespace de
#endif // DENG2_C_API_ONLY
#endif // __cplusplus

#endif /* LIBDENG_FILECLASS_H */
