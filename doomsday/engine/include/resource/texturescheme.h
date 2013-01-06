/**
 * @file texturescheme.h
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

#ifndef LIBDENG_RESOURCE_TEXTURESCHEME_H
#define LIBDENG_RESOURCE_TEXTURESCHEME_H

#include <de/PathTree>
#include "api_uri.h"
#include "resource/texturemanifest.h"

namespace de {

/**
 * TextureScheme defines a texture system subspace.
 */
class TextureScheme
{
public:
    typedef class TextureManifest Manifest;

    /// Minimum length of a symbolic name.
    static int const min_name_length = URI_MINSCHEMELENGTH;

    /// Texture manifests within the scheme are placed into a tree.
    typedef PathTreeT<Manifest> Index;

public:
    /// The requested manifest could not be found in the index.
    DENG2_ERROR(NotFoundError);

public:
    /**
     * Construct a new (empty) texture subspace scheme.
     *
     * @param symbolicName  Symbolic name of the new subspace scheme. Must
     *                      have at least @ref min_name_length characters.
     */
    explicit TextureScheme(String symbolicName);

    ~TextureScheme();

    /// @return  Symbolic name of this scheme (e.g., "ModelSkins").
    String const &name() const;

    /// @return  Total number of manifests in the scheme.
    int size() const;

    /// @return  Total number of manifests in the scheme. Same as @ref size().
    inline int count() const {
        return size();
    }

    /**
     * Clear all manifests in the scheme (any GL textures which have been
     * acquired for associated textures will be released).
     */
    void clear();

    /**
     * Insert a new manifest at the given @a path into the scheme.
     * If a manifest already exists at this path, the existing manifest is
     * returned and this is a no-op.
     *
     * @param path  Virtual path for the resultant manifest.
     * @return  The (possibly newly created) manifest at @a path.
     */
    Manifest &insertManifest(Path const &path);

    /**
     * Search the scheme for a manifest matching @a path.
     *
     * @return  Found manifest.
     */
    Manifest const &find(Path const &path) const;

    /// @copydoc find()
    Manifest &find(Path const &path);

    /**
     * Search the scheme for a manifest whose associated resource
     * URI matches @a uri.
     *
     * @return  Found manifest.
     */
    Manifest const &findByResourceUri(Uri const &uri) const;

    /// @copydoc findByResourceUri()
    Manifest &findByResourceUri(Uri const &uri);

    /**
     * Search the scheme for a manifest whose associated unique
     * identifier matches @a uniqueId.
     *
     * @return  Found manifest.
     */
    Manifest const &findByUniqueId(int uniqueId) const;

    /// @copydoc findByUniqueId()
    Manifest &findByUniqueId(int uniqueId);

    /**
     * Provides access to the manifest index for efficient traversal.
     */
    Index const &index() const;

    /// @todo Refactor away -ds
    void markUniqueIdLutDirty();

private:
    struct Instance;
    Instance *d;
};

} // namespace de

#endif /// LIBDENG_RESOURCE_TEXTURESCHEME_H
