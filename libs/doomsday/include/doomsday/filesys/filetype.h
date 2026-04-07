/**
 * @file filetype.h
 *
 * File Type. @ingroup fs
 *
 * @author Copyright &copy; 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DE_FILETYPE_H
#define DE_FILETYPE_H

#include "../libdoomsday.h"
#include "../resourceclass.h"

#ifdef __cplusplus
#ifndef DE_C_API_ONLY

#include <de/string.h>
#include <de/keymap.h>
#include <de/log.h>
#include <de/nativepath.h>
#include <de/string.h>
#include "filehandle.h"

namespace res {

using namespace de;

struct FileInfo;

/**
 * Encapsulates the properties and logics belonging to a logical
 * type of file file (e.g., Zip, PNG, WAV, etc...)
 *
 * @ingroup core
 */
class LIBDOOMSDAY_PUBLIC FileType
{
public:
    FileType(const String& _name, resourceclassid_t _defaultClass)
        : name_(_name), defaultClass_(_defaultClass)
    {}

    virtual ~FileType() {}

    /// Return the symbolic name of this file type.
    String const& name() const
    {
        return name_;
    }

    /// Return the unique identifier of the default class for this type of file.
    resourceclassid_t defaultClass() const
    {
        return defaultClass_;
    }

    /**
     * Add a new known extension to this file type. Earlier extensions
     * have priority.
     *
     * @param ext  Extension to add (including period).
     * @return  This instance.
     */
    FileType& addKnownExtension(const String& ext)
    {
        knownFileNameExtensions_.push_back(ext);
        return *this;
    }

    /**
     * Provides access to the known file name extension list for efficient
     * iteration.
     *
     * @return  List of known extensions.
     */
    StringList const& knownFileNameExtensions() const
    {
        return knownFileNameExtensions_;
    }

    /**
     * Does the file name in @a path match a known extension?
     *
     * @param path  File name/path to test.
     * @return  @c true if matched.
     */
    bool fileNameIsKnown(const String& path) const
    {
        // We require an extension for this.
        String ext = path.fileNameExtension();
        if (!ext.isEmpty())
        {
            return String::contains(knownFileNameExtensions_, ext, CaseInsensitive);
        }
        return false;
    }

private:
    /// Symbolic name for this type of file.
    String name_;

    /// Default class attributed to files of this type.
    resourceclassid_t defaultClass_;

    /// List of known extensions for this file type.
    StringList knownFileNameExtensions_;
};

/**
 * The special "null" FileType object.
 *
 * @ingroup core
 */
class LIBDOOMSDAY_PUBLIC NullFileType : public FileType
{
public:
    NullFileType() : FileType("FT_NONE",  RC_IMPLICIT)
    {}
};

/// @return  @c true= @a ftype is a "null-filetype" object (not a real file type).
inline bool isNullFileType(FileType const& ftype) {
    return !!dynamic_cast<NullFileType const*>(&ftype);
}

/**
 * Base class for all native-file types.
 */
class LIBDOOMSDAY_PUBLIC NativeFileType : public FileType
{
public:
    NativeFileType(const String& name, resourceclassid_t rclassId)
        : FileType(name, rclassId)
    {}

    /**
     * Attempt to interpret a file file of this type.
     *
     * @param hndl  Handle to the file to be interpreted.
     * @param path  VFS path to associate with the file.
     * @param info  File metadata info to attach to the file.
     *
     * @return  The interpreted file; otherwise @c 0.
     */
    virtual File1* interpret(FileHandle& hndl, String path, FileInfo const& info) const = 0;
};

/// @return  @c true= @a ftype is a NativeFileType object.
inline bool isNativeFileType(FileType const& ftype) {
    return !!dynamic_cast<NativeFileType const*>(&ftype);
}

/// Map of symbolic file type names to file types (not owned).
typedef KeyMap<String, const FileType *> FileTypes;

} // namespace res

LIBDOOMSDAY_PUBLIC void DD_AddFileType(const res::FileType &ftype);

/**
 * Lookup a FileType by symbolic name.
 *
 * @param name  Symbolic name of the type.
 * @return  FileType associated with @a name. May return a null-object.
 */
LIBDOOMSDAY_PUBLIC const res::FileType &DD_FileTypeByName(de::String name);

/**
 * Attempts to determine which "type" should be attributed to a resource, solely
 * by examining the name (e.g., a file name/path).
 *
 * @return  Type determined for this resource. May return a null-object.
 */
LIBDOOMSDAY_PUBLIC const res::FileType &DD_GuessFileTypeFromFileName(de::String name);

/// Returns the registered file types for efficient traversal.
LIBDOOMSDAY_PUBLIC res::FileTypes &DD_FileTypes();

#endif // DE_C_API_ONLY
#endif // __cplusplus

#endif /* DE_FILETYPE_H */
