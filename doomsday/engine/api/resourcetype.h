/**
 * @file resourcetype.h
 *
 * Resource Type.
 *
 * @ingroup core
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

#ifndef LIBDENG_RESOURCETYPE_H
#define LIBDENG_RESOURCETYPE_H

#ifdef __cplusplus
#ifndef DENG2_C_API_ONLY

#include <QStringList>
#include <de/Log>
#include <de/NativePath>
#include <de/String>
#include "filehandle.h"

enum resourceclassid_e;

namespace de
{
    struct FileInfo;

    /**
     * Encapsulates the properties and logics belonging to a logical
     * type of resource (e.g., Zip, PNG, WAV, etc...)
     *
     * @ingroup core
     */
    class ResourceType
    {
    public:
        typedef de::File1* (*InterpretFunc)(de::FileHandle& hndl, String path, FileInfo const& info);

    public:
        ResourceType(String _name, resourceclassid_e _defaultClass)
            : name_(_name), defaultClass_(_defaultClass)
        {}

        virtual ~ResourceType() {};

        /// Return the symbolic name of this resource type.
        String const& name() const
        {
            return name_;
        }

        /// Return the unique identifier of the default class for this type of resource.
        resourceclassid_e defaultClass() const
        {
            return defaultClass_;
        }

        /**
         * Add a new known extension to this resource type. Earlier extensions
         * have priority.
         *
         * @param ext  Extension to add (including period).
         * @return  This instance.
         */
        ResourceType& addKnownExtension(String ext)
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
        QStringList const& knownFileNameExtensions() const
        {
            return knownFileNameExtensions_;
        }

        /**
         * Does the file name in @a path match a known extension?
         *
         * @param path  File name/path to test.
         * @return  @c true if matched.
         */
        bool fileNameIsKnown(String path) const
        {
            // We require an extension for this.
            String ext = path.fileNameExtension();
            if(!ext.isEmpty())
            {
                return knownFileNameExtensions_.contains(ext, Qt::CaseInsensitive);
            }
            return false;
        }

    private:
        /// Symbolic name for this type of resource.
        String name_;

        /// Default class attributed to resources of this type.
        resourceclassid_e defaultClass_;

        /// List of known extensions for this resource type.
        QStringList knownFileNameExtensions_;
    };

    /**
     * The special "null" ResourceType object.
     *
     * @ingroup core
     */
    class NullResourceType : public ResourceType
    {
    public:
        NullResourceType() : ResourceType("RT_NONE",  RC_UNKNOWN)
        {}
    };

    /// @return  @c true= @a rtype is a "null-resourcetype" object (not a real resource type).
    inline bool isNullResourceType(ResourceType const& rtype) {
        return !!dynamic_cast<NullResourceType const*>(&rtype);
    }

    /**
     * Base class for all file resource types.
     */
    class FileResourceType : public ResourceType
    {
    public:
        FileResourceType(String name, resourceclassid_t rclassId)
            : ResourceType(name, rclassId)
        {}

        /**
         * Attempt to interpret a file resource of this type.
         *
         * @param hndl  Handle to the file to be interpreted.
         * @param path  VFS path to associate with the file.
         * @param info  File metadata info to attach to the file.
         *
         * @return  The interpreted file; otherwise @c 0.
         */
        virtual de::File1* interpret(de::FileHandle& /*hndl*/, String /*path*/, FileInfo const& /*info*/) const = 0;
    };

    /// @return  @c true= @a rtype is a FileResourceType object.
    inline bool isFileResourceType(ResourceType const& rtype) {
        return !!dynamic_cast<FileResourceType const*>(&rtype);
    }

} // namespace de
#endif // DENG2_C_API_ONLY
#endif // __cplusplus

#endif /* LIBDENG_RESOURCETYPE_H */
