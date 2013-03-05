/** @file textures.h Texture Resource Collection.
 *
 * @authors Copyright © 2010-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2010-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG_RESOURCE_TEXTURES_H
#define LIBDENG_RESOURCE_TEXTURES_H

#include "api_uri.h"

#include "Texture"
#include "TextureManifest"
#include "TextureScheme"
#include <de/Error>
#include <de/Observers>
#include <de/Path>
#include <de/String>
#include <de/PathTree>
#include <de/Vector>
#include <QList>
#include <QMap>

namespace de {

/**
 * Specialized resource collection for a set of logical textures.
 *
 * @em Clearing a texture is to 'undefine' it - any names bound to it will be
 * deleted and any GL textures acquired for it are 'released'. The logical
 * Texture instance used to represent it is also deleted.
 *
 * @em Releasing a texture will leave it defined (any names bound to it will
 * persist) but any GL textures acquired for it are 'released'. Note that the
 * logical Texture instance used to represent is NOT be deleted.
 *
 * Thus there are two general states for textures in the collection:
 *
 *   A) Declared but not defined.
 *   B) Declared and defined.
 *
 * @ingroup resource
 */
class Textures : DENG2_OBSERVES(TextureScheme, ManifestDefined),
                 DENG2_OBSERVES(TextureManifest, TextureDerived),
                 DENG2_OBSERVES(Texture, Deletion)
{
    typedef class TextureManifest Manifest;
    typedef class TextureScheme Scheme;

public:
    /// The referenced texture was not found. @ingroup errors
    DENG2_ERROR(NotFoundError);

    /// An unknown scheme was referenced. @ingroup errors
    DENG2_ERROR(UnknownSchemeError);

    /// Base class for all URI validation errors. @ingroup errors
    DENG2_ERROR(UriValidationError);

    /// The validation URI is missing the scheme component. @ingroup errors
    DENG2_SUB_ERROR(UriValidationError, UriMissingSchemeError);

    /// The validation URI is missing the path component. @ingroup errors
    DENG2_SUB_ERROR(UriValidationError, UriMissingPathError);

    /// The validation URI specifies an unknown scheme. @ingroup errors
    DENG2_SUB_ERROR(UriValidationError, UriUnknownSchemeError);

    /// The validation URI is a URN. @ingroup errors
    DENG2_SUB_ERROR(UriValidationError, UriIsUrnError);

    /**
     * ResourceClass encapsulates the properties and logics belonging to a logical
     * class of resource.
     *
     * @todo Derive from de::ResourceClass -ds
     */
    struct ResourceClass
    {
        /**
         * Interpret a manifest producing a new logical Texture instance..
         *
         * @param manifest  The manifest to be interpreted.
         * @param userData  User data to associate with the resultant texture.
         */
        static Texture *interpret(Manifest &manifest, void *userData = 0);
    };

    typedef QMap<String, Scheme *> Schemes;
    typedef QList<Texture *> All;

public:
    /**
     * Constructs a new texture resource collection.
     */
    Textures();
    virtual ~Textures();

    /// Register the console commands, variables, etc..., of this module.
    static void consoleRegister();

    /**
     * Determines if a manifest exists for a declared texture on @a path.
     * @return @c true, if a manifest exists; otherwise @a false.
     */
    bool has(Uri const &path) const;

    /**
     * Find the manifest for a declared texture.
     *
     * @param search  The search term.
     * @return Found unique identifier.
     */
    Manifest &find(Uri const &search) const;

    /**
     * Lookup a subspace scheme by symbolic name.
     *
     * @param name  Symbolic name of the scheme.
     * @return  Scheme associated with @a name.
     *
     * @throws UnknownSchemeError If @a name is unknown.
     */
    Scheme &scheme(String name) const;

    /**
     * Create a new subspace scheme.
     *
     * @note Scheme creation order defines the order in which schemes are tried
     *       by @ref find() when presented with an ambiguous URI (i.e., those
     *       without a scheme).
     *
     * @param name      Unique symbolic name of the new scheme. Must be at
     *                  least @c Scheme::min_name_length characters long.
     */
    Scheme &createScheme(String name);

    /**
     * Returns @c true iff a Scheme exists with the symbolic @a name.
     */
    bool knownScheme(String name) const;

    /**
     * Returns a list of all the schemes for efficient traversal.
     */
    Schemes const &allSchemes() const;

    /**
     * Returns the total number of manifest schemes in the collection.
     */
    inline int schemeCount() const { return allSchemes().count(); }

    /**
     * Clear all textures in all schemes.
     *
     * @see Scheme::clear().
     */
    inline void clearAllSchemes()
    {
        foreach(Scheme *scheme, allSchemes())
        {
            scheme->clear();
        }
    }

    /**
     * Declare a texture in the collection, producing a manifest for a logical
     * Texture which will be defined later. If a manifest with the specified
     * @a uri already exists the existing manifest will be returned.
     *
     * If any of the property values (flags, dimensions, etc...) differ from
     * that which is already defined in the pre-existing manifest, any texture
     * which is currently associated is released (any GL-textures acquired for
     * it are deleted).
     *
     * @param uri           Uri representing a path to the texture in the
     *                      virtual hierarchy.
     * @param flags         Texture flags property.
     * @param dimensions    Logical dimensions property.
     * @param origin        World origin offset property.
     * @param uniqueId      Unique identifier property.
     * @param resourceUri   Resource URI property.
     *
     * @return  Manifest for this URI; otherwise @c 0 if @a uri is invalid.
     */
    Manifest *declare(Uri const &uri, de::Texture::Flags flags,
                      Vector2i const &dimensions, Vector2i const &origin, int uniqueId,
                      de::Uri const *resourceUri = 0);

    /**
     * Returns a list of all the unique texture instances in the collection,
     * from all schemes.
     */
    All const &all() const;

    /**
     * Iterate over declared textures in the collection making a callback for
     * each visited. Iteration ends when all textures have been visited or a
     * callback returns non-zero.
     *
     * @param callback      Callback function ptr.
     * @param parameters    Passed to the callback.
     *
     * @return  @c 0 iff iteration completed wholly.
     */
    inline int iterateDeclared(int (*callback)(Manifest &manifest, void *parameters),
                               void* parameters = 0) const {
        return iterateDeclared("", callback, parameters);
    }

    /**
     * @copydoc iterate()
     * @param nameOfScheme  If a known symbolic scheme name, only consider
     *                      textures within this scheme. Can be @ zero-length
     *                      string, in which case visit all textures.
     */
    int iterateDeclared(String nameOfScheme, int (*callback)(Manifest &manifest, void *parameters),
                        void* parameters = 0) const;

protected:
    // Observes Scheme ManifestDefined.
    void schemeManifestDefined(Scheme &scheme, Manifest &manifest);

    // Observes Manifest MaterialDerived.
    void manifestTextureDerived(Manifest &manifest, Texture &texture);

    // Observes Texture Deletion.
    void textureBeingDeleted(Texture const &texture);

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif /* LIBDENG_RESOURCE_TEXTURES_H */
