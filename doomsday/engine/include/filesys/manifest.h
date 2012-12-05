/**
 * @file manifest.h
 *
 * Manifest. @ingroup fs
 *
 * @author Copyright &copy; 2010-2012 Daniel Swanson <danij@dengine.net>
 * @author Copyright &copy; 2010-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG_MANIFEST_H
#define LIBDENG_MANIFEST_H

#ifdef __cplusplus

#include <QStringList>

#include <de/String>
#include "uri.h"

namespace de
{
    /**
     * Stores high-level metadata for and arbitrates/facilitates
     * access to the associated "physical" resource.
     *
     * @ingroup core
     */
    class Manifest
    {
    public:
        /**
         * @param rClass    Class for the associated resource.
         * @param fFlags    @ref fileFlags
         * @param name      An expected name for the associated file.
         */
        Manifest(resourceclassid_t rClass, int fFlags, String* name = 0);
        ~Manifest();

        /// @return Class of the associated resource.
        resourceclassid_t resourceClass() const;

        /// @return Flags for this file.
        int fileFlags() const;

        /// @return List of "identity keys" used to identify the file.
        QStringList const& identityKeys() const;

        /// @return List of names for the associated file.
        QStringList const& names() const;

        /**
         * Attempt to locate this file by systematically resolving and then
         * checking each search path.
         */
        Manifest& locateFile();

        /**
         * "Forget" the currently located file if one has been found.
         */
        Manifest& forgetFile();

        /**
         * Attempt to resolve a path to (and maybe locate) this file.
         *
         * @param tryLocate  @c true= Attempt to locate the file now.
         *
         * @return Path to the found file or an empty string.
         *
         * @see locateFile()
         */
        String const& resolvedPath(bool tryLocate = true);

        /**
         * Add a new file segment identity key to the list for this manifest.
         *
         * @param newIdentityKey    New identity key (e.g., a lump/file name).
         * @param didAdd            If not @c =0, the outcome will be written here.
         */
        Manifest& addIdentityKey(String newIdentityKey, bool* didAdd = 0);

        /**
         * Add a new file name to the list of names for this manifest.
         *
         * @param newName       New name for this file. Newer names have precedence.
         * @param didAdd        If not @c =0, the outcome will be written here.
         */
        Manifest& addName(String newName, bool* didAdd = 0);

        /**
         * Print information about a file to the console.
         *
         * @param manifest      Manifest for the file.
         * @param showStatus    @c true = print loaded/located status for the
         *                      associated file.
         */
        static void consolePrint(Manifest& manifest, bool showStatus = true);

    private:
        struct Instance;
        Instance* d;
    };

} // namespace de

#endif // __cplusplus

#endif /* LIBDENG_MANIFEST_H */
