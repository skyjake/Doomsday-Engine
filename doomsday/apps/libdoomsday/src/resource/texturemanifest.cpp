/** @file texturemanifest.cpp  Texture Manifest.
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

#include "doomsday/resource/texturemanifest.h"
#include "doomsday/resource/textures.h"
#include "doomsday/resource/resources.h"

using namespace de;

namespace res {

static TextureManifest::TextureConstructor textureConstructor;

DENG2_PIMPL(TextureManifest)
, DENG2_OBSERVES(Texture, Deletion)
{
    int uniqueId;                    ///< Scheme-unique identifier (user defined).
    de::Uri resourceUri;             ///< Image resource path, to be loaded.
    Vector2ui logicalDimensions;     ///< Dimensions in map space.
    Vector2i origin;                 ///< Origin offset in map space.
    Texture::Flags flags;            ///< Classification flags.
    std::unique_ptr<Texture> texture;///< Associated resource (if any).
    TextureScheme *ownerScheme = nullptr;
    
    Impl(Public *i)
        : Base(i)
        , uniqueId(0)
    {}

    ~Impl()
    {
        if (texture) texture->audienceForDeletion -= this;
        DENG2_FOR_PUBLIC_AUDIENCE(Deletion, i) i->textureManifestBeingDeleted(self);
    }

    // Observes Texture Deletion.
    void textureBeingDeleted(Texture const & /*texture*/)
    {
        texture.release();
    }
};

TextureManifest::TextureManifest(PathTree::NodeArgs const &args)
    : Node(args)
    , d(new Impl(this))
{}

Texture *TextureManifest::derive()
{
    LOG_AS("TextureManifest::derive");
    if (!hasTexture())
    {
        DENG2_ASSERT(textureConstructor != nullptr);

        // Instantiate and associate the new texture with this.
        setTexture(textureConstructor(*this));

        // Notify interested parties that a new texture was derived from the manifest.
        DENG2_FOR_AUDIENCE(TextureDerived, i) i->textureManifestTextureDerived(*this, texture());
    }
    else
    {
        Texture *tex = &texture();

        /// @todo Materials and Surfaces should be notified of this!
        tex->setFlags(d->flags);
        tex->setDimensions(d->logicalDimensions);
        tex->setOrigin(d->origin);
    }
    return &texture();
}

void TextureManifest::setScheme(TextureScheme &ownerScheme)
{
    // Note: this pointer will only become invalid if the scheme is deleted, but in
    // that case this manifest will be deleted first anyway.
    d->ownerScheme = &ownerScheme;
}
    
TextureScheme &TextureManifest::scheme() const
{
    return *d->ownerScheme;
}

String const &TextureManifest::schemeName() const
{
    return scheme().name();
}

String TextureManifest::description(de::Uri::ComposeAsTextFlags uriCompositionFlags) const
{
    String info = String("%1 %2")
                      .arg(composeUri().compose(uriCompositionFlags | de::Uri::DecodePath),
                           ( uriCompositionFlags.testFlag(de::Uri::OmitScheme)? -14 : -22 ) )
                      .arg(sourceDescription(), -7);
#ifdef __CLIENT__
    info += String("x%1").arg(!hasTexture()? 0 : texture().variantCount());
#endif
    info += " " + (!hasResourceUri()? "N/A" : resourceUri().asText());
    return info;
}

String TextureManifest::sourceDescription() const
{
    if (!hasTexture()) return "unknown";
    if (texture().isFlagged(Texture::Custom)) return "add-on";
    return "game";
}

bool TextureManifest::hasResourceUri() const
{
    return !d->resourceUri.isEmpty();
}

de::Uri TextureManifest::resourceUri() const
{
    if (hasResourceUri())
    {
        return d->resourceUri;
    }
    /// @throw MissingResourceUriError There is no resource URI defined.
    throw MissingResourceUriError("TextureManifest::scheme", "No resource URI is defined");
}

bool TextureManifest::setResourceUri(de::Uri const &newUri)
{
    // Avoid resolving; compare as text.
    if (d->resourceUri.asText() != newUri.asText())
    {
        d->resourceUri = newUri;
        return true;
    }
    return false;
}

int TextureManifest::uniqueId() const
{
    return d->uniqueId;
}

bool TextureManifest::setUniqueId(int newUniqueId)
{
    if (d->uniqueId == newUniqueId) return false;

    d->uniqueId = newUniqueId;

    // Notify interested parties that the uniqueId has changed.
    DENG2_FOR_AUDIENCE(UniqueIdChange, i) i->textureManifestUniqueIdChanged(*this);

    return true;
}

Texture::Flags TextureManifest::flags() const
{
    return d->flags;
}

void TextureManifest::setFlags(Texture::Flags flagsToChange, FlagOp operation)
{
    applyFlagOperation(d->flags, flagsToChange, operation);
}

Vector2ui const &TextureManifest::logicalDimensions() const
{
    return d->logicalDimensions;
}

bool TextureManifest::setLogicalDimensions(Vector2ui const &newDimensions)
{
    if (d->logicalDimensions == newDimensions) return false;
    d->logicalDimensions = newDimensions;
    return true;
}

Vector2i const &TextureManifest::origin() const
{
    return d->origin;
}

void TextureManifest::setOrigin(Vector2i const &newOrigin)
{
    if (d->origin != newOrigin)
    {
        d->origin = newOrigin;
    }
}

bool TextureManifest::hasTexture() const
{
    return bool(d->texture);
}

Texture &TextureManifest::texture() const
{
    if (hasTexture())
    {
        return *d->texture;
    }
    /// @throw MissingTextureError There is no texture associated with the manifest.
    throw MissingTextureError("TextureManifest::texture", "No texture is associated");
}

void TextureManifest::setTexture(Texture *newTexture)
{
    if (d->texture.get() != newTexture)
    {
        if (d->texture)
        {
            // Cancel notifications about the existing texture.
            d->texture->audienceForDeletion -= d;
        }

        d->texture.reset(newTexture);

        if (d->texture)
        {
            // We want notification when the new texture is about to be deleted.
            d->texture->audienceForDeletion += d;
        }
    }
}

void TextureManifest::setTextureConstructor(TextureConstructor constructor)
{
    textureConstructor = constructor;
}

} // namespace res
