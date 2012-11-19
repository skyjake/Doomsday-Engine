/**
 * @file resourcemanifest.h
 * Manifest of a resource. @ingroup resource
 *
 * @author Copyright &copy; 2010-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_RESOURCERECORD_H
#define LIBDENG_RESOURCERECORD_H

#ifdef __cplusplus

#include <QStringList>

#include <de/String>
#include "uri.h"

namespace de
{
    /**
     * Stores metadata about a resource and provides a way to locate the actual
     * file(s) containing the resource. @ingroup resource
     *
     * Resources are identified both by identity keys and names:
     * - identity key might be <tt>PLAYPAL>=2048</tt>, or <tt>E1M1</tt>
     * - name might be <tt>doom.wad</tt> (the containing file)
     *
     * @todo The definition and usage of identity key and name are too
     * WAD container centric. Some more generic should be used instead,
     * perhaps URI-related/based?
     *
     * @see The term @em manifest is used with this meaning:
     * http://en.wikipedia.org/wiki/Manifest_file
     */
    class ResourceManifest
    {
    public:
        /**
         * Construct a new resource manifest.
         *
         * @param rClass    Class of resource.
         * @param rFlags    @ref resourceFlags
         * @param name      An expected name for the associated resource.
         *                  The manifest must define at least one name because
         *                  the resource can be located. Use addName() to
         *                  define more names.
         */
        ResourceManifest(resourceclassid_t rClass, int rFlags, const String& name = String());

        ~ResourceManifest();

        /// @return Class of resource.
        resourceclassid_t resourceClass() const;

        /// @return Flags for this resource (see @ref resourceFlags).
        int resourceFlags() const;

        /// @return List of "identity keys" used to identify the resource.
        QStringList identityKeys() const;

        /// @return List of names for the associated resource.
        QStringList names() const;

        /**
         * Attempt to locate this resource by systematically resolving and then
         * checking each search path.
         */
        void locate();

        /**
         * "Forget" the currently located resource if one has been found.
         */
        void forgetLocation();

        /**
         * Attempt to resolve a path to (and maybe locate) this resource.
         *
         * @param tryLocate  @c true, to attempt locating the resource now
         *                   (by calling locate()).
         *
         * @return Path to the found resource or an empty string.
         *
         * @see locate()
         */
        String resolvedPath(bool tryLocate = true);

        /**
         * Add a new resource identity key to the list for this record.
         * The key is only added if a matching one hasn't been added yet
         * (duplicates not allowed).
         *
         * @param newIdentityKey  New identity key (e.g., a lump/file name).
         *
         * @return @c true, if the key was added. @c false, if it was invalid
         * or already present.
         */
        bool addIdentityKey(String newIdentityKey);

        /**
         * Add a new resource name to the list of names for this record.
         * Duplicate names are not allowed.
         *
         * @param newName   New name for this resource. Newer names have precedence.
         *
         * @return @c true, if the name was added. @c false, if it was invalid
         * or already present.
         */
        bool addName(String newName);

        /**
         * Print information about the resource to the console.
         *
         * @param showStatus    @c true = print loaded/located status for the
         *                      associated resource.
         */
        void print(bool showStatus = true);

    private:
        struct Instance;
        Instance* d;
    };

} // namespace de

#endif // __cplusplus

#endif /* LIBDENG_RESOURCERECORD_H */
