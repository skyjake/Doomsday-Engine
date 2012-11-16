/**
 * @file resourcerecord.h
 *
 * Resource Record.
 *
 * @ingroup resource
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef LIBDENG_RESOURCERECORD_H
#define LIBDENG_RESOURCERECORD_H

#include <QStringList>

#include <de/String>
#include "uri.h"

namespace de
{
    /**
     * ResourceRecord. Stores high-level metadata for and manages a logical resource.
     *
     * @ingroup core
     */
    class ResourceRecord
    {
    public:
        /**
         * @param rClass    Class of resource.
         * @param rFlags    @ref resourceFlags
         * @param name      An expected name for the associated resource.
         */
        ResourceRecord(resourceclassid_t rClass, int rFlags, String* name = 0);
        ~ResourceRecord();

        /// @return Class of resource.
        resourceclassid_t resourceClass() const;

        /// @return Flags for this resource.
        int resourceFlags() const;

        /// @return List of "identity keys" used to identify the resource.
        QStringList const& identityKeys() const;

        /// @return List of names for the associated resource.
        QStringList const& names() const;

        /**
         * Attempt to locate this resource by systematically resolving and then
         * checking each search path.
         */
        ResourceRecord& locateResource();

        /**
         * "Forget" the currently located resource if one has been found.
         */
        ResourceRecord& forgetResource();

        /**
         * Attempt to resolve a path to (and maybe locate) this resource.
         *
         * @param tryLocate  Attempt to locate the resource now.
         *
         * @return Path to the found resource or an empty string.
         *
         * @see locateResource()
         */
        String const& resolvedPath(bool tryLocate = true);

        /**
         * Add a new sub-resource identity key to the list for this record.
         *
         * @param newIdentityKey    New identity key (e.g., a lump/file name).
         * @param didAdd            If not @c =0, the outcome will be written here.
         */
        ResourceRecord& addIdentityKey(String newIdentityKey, bool* didAdd = 0);

        /**
         * Add a new resource name to the list of names for this record.
         *
         * @param newName       New name for this resource. Newer names have precedence.
         * @param didAdd        If not @c =0, the outcome will be written here.
         */
        ResourceRecord& addName(String newName, bool* didAdd = 0);

        /**
         * Print information about a resource to the console.
         *
         * @param record        Record for the resource.
         * @param showStatus    @c true = print loaded/located status for the
         *                      associated resource.
         */
        static void consolePrint(ResourceRecord& record, bool showStatus = true);

    private:
        struct Instance;
        Instance* d;
    };

} // namespace de

#endif /* LIBDENG_RESOURCERECORD_H */
