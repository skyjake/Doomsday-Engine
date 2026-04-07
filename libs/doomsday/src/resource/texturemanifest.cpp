#include <utility>

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

#include "doomsday/res/texturemanifest.h"
#include "doomsday/res/textures.h"
#include "doomsday/res/resources.h"

using namespace de;

namespace res {

static TextureManifest::TextureConstructor textureConstructor;

DE_PIMPL(TextureManifest)
, DE_OBSERVES(Texture, Deletion)
{
    int                      uniqueId;          ///< Scheme-unique identifier (user defined).
    res::Uri                  resourceUri;       ///< Image resource path, to be loaded.
    Vec2ui                   logicalDimensions; ///< Dimensions in map space.
    Vec2i                    origin;            ///< Origin offset in map space.
    Flags                    flags;             ///< Classification flags.
    std::unique_ptr<Texture> texture;           ///< Associated resource (if any).
    TextureScheme *          ownerScheme = nullptr;

    Impl(Public *i)
        : Base(i)
        , uniqueId(0)
    {}

    ~Impl()
    {
        if (texture) texture->audienceForDeletion -= this;
        DE_NOTIFY_PUBLIC_VAR(Deletion, i) i->textureManifestBeingDeleted(self());
    }

    // Observes Texture Deletion.
    void textureBeingDeleted(const Texture & /*texture*/)
    {
        texture.release();
    }
};

TextureManifest::TextureManifest(const PathTree::NodeArgs &args)
    : Node(args)
    , d(new Impl(this))
{}

Texture *TextureManifest::derive()
{
    LOG_AS("TextureManifest::derive");
    if (!hasTexture())
    {
        DE_ASSERT(textureConstructor != nullptr);

        // Instantiate and associate the new texture with this.
        setTexture(textureConstructor(*this));

        // Notify interested parties that a new texture was derived from the manifest.
        DE_NOTIFY_VAR(TextureDerived, i) i->textureManifestTextureDerived(*this, texture());
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

const String &TextureManifest::schemeName() const
{
    return scheme().name();
}

String TextureManifest::description(res::Uri::ComposeAsTextFlags uriCompositionFlags) const
{
//    String info = String("%1 %2")
//                      .arg(composeUri().compose(uriCompositionFlags | res::Uri::DecodePath),
//                           ( uriCompositionFlags.testFlag(res::Uri::OmitScheme)? -14 : -22 ) )
//                      .arg(sourceDescription(), -7);

    String info =
        composeUri().compose(uriCompositionFlags | res::Uri::DecodePath) + " " + sourceDescription();

//#ifdef __CLIENT__
//    info += Stringf("x%i", !hasTexture()? 0 : texture().variantCount());
//#endif
    info += " ";
    info += (!hasResourceUri()? "N/A" : resourceUri().asText());
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

res::Uri TextureManifest::resourceUri() const
{
    if (hasResourceUri())
    {
        return d->resourceUri;
    }
    /// @throw MissingResourceUriError There is no resource URI defined.
    throw MissingResourceUriError("TextureManifest::scheme", "No resource URI is defined");
}

bool TextureManifest::setResourceUri(const res::Uri &newUri)
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
    DE_NOTIFY_VAR(UniqueIdChange, i) i->textureManifestUniqueIdChanged(*this);

    return true;
}

Flags TextureManifest::flags() const
{
    return d->flags;
}

void TextureManifest::setFlags(Flags flagsToChange, FlagOp operation)
{
    applyFlagOperation(d->flags, flagsToChange, operation);
}

const Vec2ui &TextureManifest::logicalDimensions() const
{
    return d->logicalDimensions;
}

bool TextureManifest::setLogicalDimensions(const Vec2ui &newDimensions)
{
    if (d->logicalDimensions == newDimensions) return false;
    d->logicalDimensions = newDimensions;
    return true;
}

const Vec2i &TextureManifest::origin() const
{
    return d->origin;
}

void TextureManifest::setOrigin(const Vec2i &newOrigin)
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

Texture *TextureManifest::texturePtr() const
{
    return d->texture.get();
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
    textureConstructor = std::move(constructor);
}

} // namespace res
