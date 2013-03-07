/** @file manifest.h Logical Resource Manifest
 *
 * @author Copyright Â© 2010-2013 Daniel Swanson <danij@dengine.net>
 * @author Copyright Â© 2010-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG_RESOURCE_MANIFEST_H
#define LIBDENG_RESOURCE_MANIFEST_H

#include <de/String>
#include <QStringList>
#include "api_uri.h"

namespace de {

/**
 * Stores high-level metadata for and arbitrates/facilitates access to the
 * associated "physical" game data resource.
 *
 * @ingroup core
 */
class ResourceManifest
{
public:
    /**
     * @param rClass    Class for the associated resource.
     * @param fFlags    @ref fileFlags
     * @param name      An expected name for the associated file.
     */
    ResourceManifest(resourceclassid_t rClass, int fFlags, String *name = 0);

    /// @return Class of the associated resource.
    resourceclassid_t resourceClass() const;

    /// @return Flags for this file.
    int fileFlags() const;

    /// @return List of "identity keys" used to identify the file.
    QStringList const &identityKeys() const;

    /// @return List of names for the associated file.
    QStringList const &names() const;

    /**
     * Attempt to locate this file by systematically resolving and then
     * checking each search path.
     */
    ResourceManifest &locateFile();

    /**
     * "Forget" the currently located file if one has been found.
     */
    ResourceManifest &forgetFile();

    /**
     * Attempt to resolve a path to (and maybe locate) this file.
     *
     * @param tryLocate  @c true= Attempt to locate the file now.
     *
     * @return Path to the found file or an empty string.
     *
     * @see locateFile()
     */
    String const &resolvedPath(bool tryLocate = true);

    /**
     * Add a new file segment identity key to the list for this manifest.
     *
     * @param newIdentityKey    New identity key (e.g., a lump/file name).
     * @param didAdd            If not @c =0, the outcome will be written here.
     */
    ResourceManifest &addIdentityKey(String newIdentityKey, bool *didAdd = 0);

    /**
     * Add a new file name to the list of names for this manifest.
     *
     * @param newName       New name for this file. Newer names have precedence.
     * @param didAdd        If not @c =0, the outcome will be written here.
     */
    ResourceManifest &addName(String newName, bool *didAdd = 0);

    /**
     * Print information about a file to the console.
     *
     * @param manifest      Manifest for the file.
     * @param showStatus    @c true = print loaded/located status for the
     *                      associated file.
     */
    static void consolePrint(ResourceManifest &manifest, bool showStatus = true);

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif /* LIBDENG_RESOURCE_MANIFEST_H */
