/** @file texturemanifest.cpp Texture Manifest.
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

#include "de_base.h" // App_Textures()
#include "Textures"

#include "resource/texturemanifest.h"

using namespace de;

DENG2_PIMPL(TextureManifest)
{
    /// Scheme-unique identifier determined by the owner of the subspace.
    int uniqueId;

    /// Path to the resource containing the image data to be loaded.
    Uri resourceUri;

    /// World dimensions in map coordinate space units.
    Vector2i logicalDimensions;

    /// World origin offset in map coordinate space units.
    Vector2i origin;

    /// Texture classification flags.
    Texture::Flags flags;

    /// The associated logical Texture instance (if any).
    std::auto_ptr<Texture> texture;

    Instance(Public *i) : Base(i), uniqueId(0)
    {}
};

TextureManifest::TextureManifest(PathTree::NodeArgs const &args)
    : Node(args), d(new Instance(this))
{}

Textures &TextureManifest::textures()
{
    return App_Textures();
}

Texture *TextureManifest::derive()
{
    LOG_AS("TextureManifest::derive");
    if(hasTexture())
    {
#if _DEBUG
        LOG_INFO("\"%s\" already has an existing texture, reconfiguring.") << composeUri();
#endif
        Texture *tex = &texture();

        tex->setFlags(d->flags);
        tex->setDimensions(d->logicalDimensions);
        tex->setOrigin(d->origin);

        /// @todo Materials and Surfaces should be notified of this!
        return tex;
    }

    Texture *tex = Textures::ResourceClass::interpret(*this);
    if(!hasTexture()) setTexture(tex);
    return tex;
}

TextureScheme &TextureManifest::scheme() const
{
    LOG_AS("TextureManifest::scheme");
    /// @todo Optimize: TextureManifest should contain a link to the owning Textures::Scheme.
    foreach(TextureScheme *scheme, textures().allSchemes())
    {
        if(&scheme->index() == &tree()) return *scheme;
    }
    /// @throw Error Failed to determine the scheme of the manifest (should never happen...).
    throw Error("TextureManifest::scheme", String("Failed to determine scheme for manifest [%1]").arg(de::dintptr(this)));
}

String TextureManifest::description(de::Uri::ComposeAsTextFlags uriCompositionFlags) const
{
    String info = String("%1 %2")
                      .arg(composeUri().compose(uriCompositionFlags | Uri::DecodePath),
                           ( uriCompositionFlags.testFlag(Uri::OmitScheme)? -14 : -22 ) )
                      .arg(sourceDescription(), -7);
#ifdef __CLIENT__
    info += String("x%1").arg(!hasTexture()? 0 : texture().variantCount());
#endif
    info += " " + (!hasResourceUri()? "N/A" : resourceUri().asText());
    return info;
}

String TextureManifest::sourceDescription() const
{
    if(!hasTexture()) return "unknown";
    if(texture().isFlagged(Texture::Custom)) return "add-on";
    return "game";
}

bool TextureManifest::hasResourceUri() const
{
    return !d->resourceUri.isEmpty();
}

de::Uri TextureManifest::resourceUri() const
{
    if(hasResourceUri())
    {
        return d->resourceUri;
    }
    /// @throw MissingResourceUriError There is no resource URI defined.
    throw MissingResourceUriError("TextureManifest::scheme", "No resource URI is defined");
}

bool TextureManifest::setResourceUri(de::Uri const &newUri)
{
    // Avoid resolving; compare as text.
    if(d->resourceUri.asText() != newUri.asText())
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
    if(d->uniqueId == newUniqueId) return false;

    d->uniqueId = newUniqueId;
    // We'll need to rebuild the id map too.
    scheme().markUniqueIdLutDirty();
    return true;
}

Texture::Flags TextureManifest::flags() const
{
    return d->flags;
}

Vector2i const &TextureManifest::logicalDimensions() const
{
    return d->logicalDimensions;
}

bool TextureManifest::setLogicalDimensions(Vector2i const &newDimensions)
{
    if(d->logicalDimensions == newDimensions) return false;
    d->logicalDimensions = newDimensions;
    return true;
}

Vector2i const &TextureManifest::origin() const
{
    return d->origin;
}

void TextureManifest::setOrigin(Vector2i const &newOrigin)
{
    if(d->origin != newOrigin)
    {
        d->origin = newOrigin;
    }
}

bool TextureManifest::hasTexture() const
{
    return !!d->texture.get();
}

Texture &TextureManifest::texture() const
{
    if(hasTexture())
    {
        return *d->texture.get();
    }
    /// @throw MissingTextureError There is no texture associated with the manifest.
    throw MissingTextureError("TextureManifest::texture", "No texture is associated");
}

void TextureManifest::setTexture(Texture *newTexture)
{
    d->texture.reset(newTexture);
}
