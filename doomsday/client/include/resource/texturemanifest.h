/** @file texturemanifest.h Texture Manifest.
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

#ifndef LIBDENG_RESOURCE_TEXTUREMANIFEST_H
#define LIBDENG_RESOURCE_TEXTUREMANIFEST_H

#include "Texture"
#include "uri.hh"
#include <de/PathTree>
#include <QSize>

namespace de {

class Textures;
class TextureScheme;

/**
 * Metadata for a would-be logical Texture resource.
 *
 * Models a reference to and the associated metadata for a logical texture
 * in the texture resource collection.
 *
 * @ingroup resource
 */
class TextureManifest : public PathTree::Node
{
public:
    TextureManifest(PathTree::NodeArgs const &args);
    virtual ~TextureManifest();

    /**
     * Derive a new logical Texture instance by interpreting the manifest.
     * The first time a texture is derived from the manifest, said texture
     * is assigned to the manifest (ownership is assumed).
     */
    Texture *derive();

    /**
     * Returns the owning scheme of the TextureManifest.
     */
    TextureScheme &scheme() const;

    /// Convenience method for returning the name of the owning scheme.
    String const &schemeName() const;

    /**
     * Compose a URI of the form "scheme:path" for the TextureManifest.
     *
     * The scheme component of the URI will contain the symbolic name of
     * the scheme for the TextureManifest.
     *
     * The path component of the URI will contain the percent-encoded path
     * of the TextureManifest.
     */
    Uri composeUri(QChar sep = '/') const;

    /**
     * Compose a URN of the form "urn:scheme:uniqueid" for the texture
     * TextureManifest.
     *
     * The scheme component of the URI will contain the identifier 'urn'.
     *
     * The path component of the URI is a string which contains both the
     * symbolic name of the scheme followed by the unique id of the texture
     * TextureManifest, separated with a colon.
     *
     * @see uniqueId(), setUniqueId()
     */
    Uri composeUrn() const;

    /**
     * Returns the URI to the associated resource. May be empty.
     */
    Uri const &resourceUri() const;

    /**
     * Change the resource URI associated with the manifest.
     *
     * @return  @c true iff @a newUri differed to the existing URI, which
     *          was subsequently changed.
     */
    bool setResourceUri(Uri const &newUri);

    /**
     * Returns the scheme-unique identifier for the manifest.
     */
    int uniqueId() const;

    /**
     * Change the unique identifier property of the manifest.
     *
     * @return  @c true iff @a newUniqueId differed to the existing unique
     *          identifier, which was subsequently changed.
     */
    bool setUniqueId(int newUniqueId);

    /**
     * Returns the logical dimensions property of the manifest.
     */
    QSize const &logicalDimensions() const;

    /**
     * Change the logical dimensions property of the manifest.
     *
     * @param newDimensions  New logical dimensions. Components can be @c 0 in
     * which case their value will be inherited from the pixel dimensions of
     * the image at load time.
     */
    bool setLogicalDimensions(QSize const &newDimensions);

    /**
     * Returns the world origin offset property of the manifest.
     */
    QPoint const &origin() const;

    /**
     * Change the world origin offest property of the manifest.
     *
     * @param newOrigin  New origin offset.
     */
    void setOrigin(QPoint const &newOrigin);

    /**
     * Returns the texture flags property of the manifest.
     */
    Texture::Flags &flags();

    /**
     * Returns the logical Texture instance associated with the manifest;
     * otherwise @c 0.
     */
    Texture *texture() const;

    /**
     * Change the logical Texture associated with the manifest.
     *
     * @param newTexture  New logical Texture to associate.
     */
    void setTexture(Texture *newTexture);

    /// Returns a reference to the application's texture system.
    static Textures &textures();

private:
    struct Instance;
    Instance *d;
};

} // namespace de

#endif /// LIBDENG_RESOURCE_TEXTUREMANIFEST_H
