/** @file textures.h
 *
 * @authors Copyright (c) 2016 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDOOMSDAY_RESOURCE_TEXTURES_H
#define LIBDOOMSDAY_RESOURCE_TEXTURES_H

#include "texturescheme.h"
#include "texture.h"

namespace res {

class LIBDOOMSDAY_PUBLIC Textures
{
public:
    typedef QMap<de::String, TextureScheme *> TextureSchemes;
    typedef QList<Texture *> AllTextures;

    static Textures &get();

public:
    Textures();

    void clear();

    /**
     * Determines if a texture exists for @a path.
     *
     * @return @c true, if a texture exists; otherwise @a false.
     *
     * @see hasTextureManifest(), TextureManifest::hasTexture()
     */
    inline bool hasTexture(de::Uri const &path) const {
        if (hasTextureManifest(path)) return textureManifest(path).hasTexture();
        return false;
    }

    /**
     * Lookup a texture resource for the specified @a path.
     *
     * @return The found texture.
     *
     * @see textureManifest(), TextureManifest::texture()
     */
    inline Texture &texture(de::Uri const &path) const {
        return textureManifest(path).texture();
    }

    /**
     * Returns a pointer to the identified Texture.
     *
     * @see hasTextureManifest(), TextureManifest::texturePtr()
     */
    inline Texture *texturePtr(de::Uri const &path) {
        if (hasTextureManifest(path)) return textureManifest(path).texturePtr();
        return nullptr;
    }

    /**
     * Convenient method of searching the texture collection for a texture with
     * the specified @a schemeName and @a resourceUri.
     *
     * @param schemeName   Unique name of the scheme in which to search.
     * @param resourceUri  Path to the (image) resource to find the texture for.
     *
     * @return  The found texture; otherwise @c nullptr.
     */
    Texture *texture(de::String schemeName, de::Uri const &resourceUri);

    /**
     * Determines if a texture manifest exists for a declared texture on @a path.
     *
     * @return @c true, if a manifest exists; otherwise @a false.
     */
    bool hasTextureManifest(de::Uri const &path) const;

    /**
     * Find the manifest for a declared texture.
     *
     * @param search  The search term.
     * @return Found unique identifier.
     */
    TextureManifest &textureManifest(de::Uri const &search) const;

    /**
     * Lookup a subspace scheme by symbolic name.
     *
     * @param name  Symbolic name of the scheme.
     * @return  Scheme associated with @a name.
     *
     * @throws UnknownSchemeError If @a name is unknown.
     */
    TextureScheme &textureScheme(de::String name) const;

    /**
     * Returns @c true iff a Scheme exists with the symbolic @a name.
     */
    bool isKnownTextureScheme(de::String name) const;

    /**
     * Returns a list of all the schemes for efficient traversal.
     */
    TextureSchemes const &allTextureSchemes() const;

    /**
     * Returns the total number of manifest schemes in the collection.
     */
    inline de::dint textureSchemeCount() const {
        return allTextureSchemes().count();
    }

    /**
     * Clear all textures in all schemes.
     *
     * @see Scheme::clear().
     */
    void clearAllTextureSchemes();

    /**
     * Returns a list of all the unique texture instances in the collection,
     * from all schemes.
     */
    AllTextures const &allTextures() const;

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
     * @return  Manifest for this URI.
     */
    inline TextureManifest &declareTexture(
            de::Uri const &uri,
            Texture::Flags flags,
            de::Vector2ui const &dimensions,
            de::Vector2i const &origin,
            de::dint uniqueId,
            de::Uri const *resourceUri = nullptr)
    {
        return textureScheme(uri.scheme())
                   .declare(uri.path(), flags, dimensions, origin, uniqueId,
                            resourceUri);
    }

    Texture *defineTexture(de::String schemeName,
                           de::Uri const &resourceUri,
                           de::Vector2ui const &dimensions = de::Vector2ui());

    /**
     * Ensure a texture has been derived for @a manifest.
     * @param manifest Manifest.
     * @return Derived texture object.
     */
    Texture *deriveTexture(TextureManifest &manifest);

    void deriveAllTexturesInScheme(de::String schemeName);

private:
    DENG2_PRIVATE(d)
};

} // namespace res

#endif // LIBDOOMSDAY_RESOURCE_TEXTURES_H
