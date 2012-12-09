/** @file texturemanifest.cpp Texture Manifest.
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

#include "resource/textures.h"
#include "resource/texturemanifest.h"

namespace de {

struct TextureManifest::Instance
{
    /// Scheme-unique identifier determined by the owner of the subspace.
    int uniqueId;

    /// Path to the resource containing the loadable data.
    Uri resourceUri;

    /// The associated logical Texture instance (if any).
    Texture *texture;

    Instance() : uniqueId(0), resourceUri(), texture(0)
    {}

    ~Instance()
    {
        delete texture;
    }
};

TextureManifest::TextureManifest(PathTree::NodeArgs const &args) : Node(args)
{
    d = new Instance();
}

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
    return *App_Textures();
}

Texture *TextureManifest::derive(QSize const &dimensions, Texture::Flags flags)
{
    LOG_AS("TextureManifest::derive");
    if(Texture *tex = texture())
    {
#if _DEBUG
        LOG_INFO("\"%s\" already has an existing texture, reconfiguring.") << composeUri();
#endif
        tex->setDimensions(dimensions);
        tex->flags() = flags;

        /// @todo Materials and Surfaces should be notified of this!
        return tex;
    }

    Texture *tex = Textures::ResourceClass::interpret(*this, dimensions, flags);
    if(!texture()) setTexture(tex);
    return tex;
}

Texture *TextureManifest::derive(Texture::Flags flags)
{
    return derive(QSize(), flags);
}

Textures::Scheme &TextureManifest::scheme() const
{
    LOG_AS("TextureManifest::scheme");
    /// @todo Optimize: TextureManifest should contain a link to the owning Textures::Scheme.
    Textures::Schemes const &schemes = textures().allSchemes();
    DENG2_FOR_EACH_CONST(Textures::Schemes, i, schemes)
    {
        Textures::Scheme &scheme = **i;
        if(&scheme.index() == &tree()) return scheme;
    }

    // This should never happen...
    /// @throw Error Failed to determine the scheme of the manifest.
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

Texture *TextureManifest::texture() const
{
    return d->texture;
}

void TextureManifest::setTexture(Texture *newTexture)
{
    d->texture = newTexture;
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

} // namespace de
