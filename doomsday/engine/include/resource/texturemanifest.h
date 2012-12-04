/**
 * @file texturemanifest.h
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

#ifndef LIBDENG_RESOURCE_TEXTUREMANIFEST_H
#define LIBDENG_RESOURCE_TEXTUREMANIFEST_H

#include <QList>
#include <de/PathTree>
#include <de/size.h>
#include "uri.hh"
#include "resource/texture.h"

namespace de {

class Textures;

/**
 * Models a texture reference and the associated metadata for a resource
 * in the Textures collection.
 */
class TextureManifest : public PathTree::Node
{
public:
    TextureManifest(PathTree::NodeArgs const &args);
    virtual ~TextureManifest();

    /**
     * Interpret the TextureManifest creating a new logical Texture instance.
     *
     * @param dimensions  Logical dimensions. Components can be @c 0 in which
     *                  case their value will be inherited from the actual
     *                  pixel dimensions of the image at load time.
     * @param flags     Flags.
     * @param userData  User data to associate with the resultant texture.
     */
    Texture *define(Size2Raw const &dimensions, Texture::Flags flags);

    /**
     * @copydoc define()
     */
    Texture *define(Texture::Flags flags);

    /**
     * Returns the owning scheme of the TextureManifest.
     */
    Textures::Scheme &scheme() const;

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

    /**
     * Returns the scheme-unique identifier for the manifest.
     */
    int uniqueId() const;

    /**
     * Change the unique identifier associated with the manifest.
     *
     * @return  @c true iff @a newUniqueId differed to the existing unique
     *          identifier, which was subsequently changed.
     */
    bool setUniqueId(int newUniqueId);

    /// Returns a reference to the application's texture system.
    static Textures &textures();

    /// @todo Refactor away -ds
    textureid_t lookupTextureId() const;

private:
    /// Scheme-unique identifier determined by the owner of the subspace.
    int uniqueId_;

    /// Path to the resource containing the loadable data.
    Uri resourceUri_;

    /// The associated logical Texture instance (if any).
    Texture *texture_;
};

} // namespace de

#endif /// LIBDENG_RESOURCE_TEXTUREMANIFEST_H
