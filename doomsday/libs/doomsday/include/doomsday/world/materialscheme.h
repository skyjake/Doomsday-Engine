/** @file materialscheme.h  Material collection subspace.
 *
 * @authors Copyright © 2010-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDOOMSDAY_WORLD_MATERIALSCHEME_H
#define LIBDOOMSDAY_WORLD_MATERIALSCHEME_H

#include <de/error.h>
#include <de/observers.h>
#include <de/pathtree.h>
#include "../uri.h"
#include "materialmanifest.h"

namespace world {

/**
 * Material collection subspace.
 *
 * @see Material
 * @ingroup resource
 */
class LIBDOOMSDAY_PUBLIC MaterialScheme
{
    typedef class MaterialManifest Manifest;

public:
    /// The requested manifest could not be found in the index. @ingroup errors
    DE_ERROR(NotFoundError);

    /// The specified path was not valid. @ingroup errors
    DE_ERROR(InvalidPathError);

    DE_DEFINE_AUDIENCE(ManifestDefined, void materialSchemeManifestDefined(MaterialScheme &scheme, Manifest &manifest))

    /// Minimum length of a symbolic name.
    static const int min_name_length = DE_URI_MIN_SCHEME_LENGTH;

    /// Manifests in the scheme are placed into a tree.
    typedef de::PathTreeT<Manifest> Index;

public:
    /**
     * Construct a new (empty) material subspace scheme.
     *
     * @param symbolicName  Symbolic name of the new subspace scheme. Must
     *                      have at least @ref min_name_length characters.
     */
    explicit MaterialScheme(de::String symbolicName);

    /// @return  Symbolic name of this scheme (e.g., "Flats").
    const de::String &name() const;

    /// @return  Total number of manifests in the scheme.
    inline int size() const { return index().size(); }

    /// @return  Total number of manifests in the scheme. Same as @ref size().
    inline int count() const { return size(); }

    /**
     * Clear all manifests in the scheme.
     */
    void clear();

    /**
     * Insert a new manifest at the given @a path into the scheme.
     * If a manifest already exists at this path, the existing manifest is
     * returned and the call is a no-op.
     *
     * @param path  Virtual path for the resultant manifest.
     * @return  The (possibly newly created) manifest at @a path.
     */
    Manifest &declare(const de::Path &path);

    /**
     * Determines if a manifest exists on the given @a path.
     * @return @c true if a manifest exists; otherwise @a false.
     */
    bool has(const de::Path &path) const;

    /**
     * Search the scheme for a manifest matching @a path.
     *
     * @return  Found manifest.
     */
    const Manifest &find(const de::Path &path) const;

    /// @copydoc find()
    Manifest &find(const de::Path &path);

    Manifest *tryFind(const de::Path &path) const;

    /**
     * Provides access to the manifest index for efficient traversal.
     */
    const Index &index() const;

private:
    DE_PRIVATE(d)
};

} // namespace world

#endif // LIBDOOMSDAY_WORLD_MATERIALSCHEME_H
