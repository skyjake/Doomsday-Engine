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

TextureManifest::TextureManifest(PathTree::NodeArgs const &args) : Node(args),
    uniqueId_(0), resourceUri_(), texture_(0)
{}

TextureManifest::~TextureManifest()
{
    LOG_AS("~TextureManifest");
    if(texture_)
    {
#if _DEBUG
        LOG_WARNING("\"%s\" still has an associated texture!")
            << composeUri();
#endif
        delete texture_;
    }
}

Textures &TextureManifest::textures()
{
    return *App_Textures();
}

Texture *TextureManifest::derive(QSize const &dimensions, Texture::Flags flags)
{
    if(Texture *tex = texture())
    {
#if _DEBUG
        LOG_WARNING("A Texture with uri \"%s\" already exists, returning existing.")
            << composeUri();
#endif
        tex->setDimensions(dimensions);
        tex->flagCustom(!!(flags & Texture::Custom));

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
    return Uri("urn", String("%1:%2").arg(scheme().name()).arg(uniqueId_, 0, 10));
}

Uri const &TextureManifest::resourceUri() const
{
    return resourceUri_;
}

bool TextureManifest::setResourceUri(Uri const &newUri)
{
    // Avoid resolving; compare as text.
    if(resourceUri_.asText() != newUri.asText())
    {
        resourceUri_ = newUri;
        return true;
    }
    return false;
}

Texture *TextureManifest::texture() const
{
    return texture_;
}

void TextureManifest::setTexture(Texture *newTexture)
{
    texture_ = newTexture;
}

int TextureManifest::uniqueId() const
{
    return uniqueId_;
}

bool TextureManifest::setUniqueId(int newUniqueId)
{
    if(uniqueId_ == newUniqueId) return false;

    uniqueId_ = newUniqueId;
    // We'll need to rebuild the id map too.
    scheme().markUniqueIdLutDirty();
    return true;
}

} // namespace de
