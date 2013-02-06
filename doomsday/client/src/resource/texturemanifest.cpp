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

#include "de_base.h"

#include "Textures"
#include "TextureManifest"

namespace de {

struct TextureManifest::Instance
{
    /// Scheme-unique identifier determined by the owner of the subspace.
    int uniqueId;

    /// Path to the resource containing the loadable data.
    Uri resourceUri;

    /// World dimensions in map coordinate space units.
    QSize logicalDimensions;

    /// World origin offset in map coordinate space units.
    QPoint origin;

    /// Texture classification flags.
    Texture::Flags flags;

    /// The associated logical Texture instance (if any).
    Texture *texture;

    Instance() : uniqueId(0), resourceUri(), texture(0)
    {}

    ~Instance()
    {
        delete texture;
    }
};

TextureManifest::TextureManifest(PathTree::NodeArgs const &args)
    : Node(args), d(new Instance())
{}

TextureManifest::~TextureManifest()
{
    LOG_AS("~TextureManifest");
    if(d->texture)
    {
#if _DEBUG
        LOG_WARNING("\"%s\" still has an associated texture!") << composeUri();
#endif
    }
    delete d;
}

Textures &TextureManifest::textures()
{
    return App_Textures();
}

Texture *TextureManifest::derive()
{
    LOG_AS("TextureManifest::derive");
    if(Texture *tex = texture())
    {
#if _DEBUG
        LOG_INFO("\"%s\" already has an existing texture, reconfiguring.") << composeUri();
#endif
        tex->flags() = d->flags;
        tex->setDimensions(d->logicalDimensions);
        tex->setOrigin(d->origin);

        /// @todo Materials and Surfaces should be notified of this!
        return tex;
    }

    Texture *tex = Textures::ResourceClass::interpret(*this);
    if(!texture()) setTexture(tex);
    return tex;
}

Textures::Scheme &TextureManifest::scheme() const
{
    LOG_AS("TextureManifest::scheme");
    /// @todo Optimize: TextureManifest should contain a link to the owning Textures::Scheme.
    foreach(Textures::Scheme *scheme, textures().allSchemes())
    {
        if(&scheme->index() == &tree()) return *scheme;
    }
    /// @throw Error Failed to determine the scheme of the manifest (should never happen...).
    throw Error("TextureManifest::scheme", String("Failed to determine scheme for manifest [%p].").arg(de::dintptr(this)));
}

String const &TextureManifest::schemeName() const
{
    return scheme().name();
}

Uri TextureManifest::composeUri(QChar sep) const
{
    return Uri(scheme().name(), path(sep));
}

Uri TextureManifest::composeUrn() const
{
    return Uri("urn", String("%1:%2").arg(scheme().name()).arg(d->uniqueId, 0, 10));
}

Uri const &TextureManifest::resourceUri() const
{
    return d->resourceUri;
}

bool TextureManifest::setResourceUri(Uri const &newUri)
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

Texture::Flags &TextureManifest::flags()
{
    return d->flags;
}

QSize const &TextureManifest::logicalDimensions() const
{
    return d->logicalDimensions;
}

bool TextureManifest::setLogicalDimensions(QSize const &newDimensions)
{
    if(d->logicalDimensions == newDimensions) return false;
    d->logicalDimensions = newDimensions;
    return true;
}

QPoint const &TextureManifest::origin() const
{
    return d->origin;
}

void TextureManifest::setOrigin(QPoint const &newOrigin)
{
    d->origin = newOrigin;
}

Texture *TextureManifest::texture() const
{
    return d->texture;
}

void TextureManifest::setTexture(Texture *newTexture)
{
    d->texture = newTexture;
}

} // namespace de
