/** @file fontmanifest.cpp Font resource manifest.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

#include "resource/fontmanifest.h"
#include "dd_main.h" // App_Fonts()
#include "FontScheme"
#include <de/Log>

namespace de {

FontManifest::FontManifest(PathTree::NodeArgs const &args)
    : Node(args)
    , _uniqueId(0)
{}

FontManifest::~FontManifest()
{
    DENG2_FOR_AUDIENCE(Deletion, i) i->manifestBeingDeleted(*this);
}

Fonts &FontManifest::fonts()
{
    return App_Fonts();
}

FontScheme &FontManifest::scheme() const
{
    LOG_AS("FontManifest::scheme");
    /// @todo Optimize: FontRecord should contain a link to the owning FontScheme.
    foreach(FontScheme *scheme, fonts().allSchemes())
    {
        if(&scheme->index() == &tree()) return *scheme;
    }
    /// @throw Error Failed to determine the scheme of the manifest (should never happen...).
    throw Error("FontManifest::scheme", String("Failed to determine scheme for manifest [%1]").arg(de::dintptr(this)));
}

String const &FontManifest::schemeName() const
{
    return scheme().name();
}

String FontManifest::description(de::Uri::ComposeAsTextFlags uriCompositionFlags) const
{
    return String("%1").arg(composeUri().compose(uriCompositionFlags | Uri::DecodePath),
                            ( uriCompositionFlags.testFlag(Uri::OmitScheme)? -14 : -22 ) );
}

int FontManifest::uniqueId() const
{
    return _uniqueId;
}

bool FontManifest::setUniqueId(int newUniqueId)
{
    if(_uniqueId == newUniqueId) return false;

    _uniqueId = newUniqueId;

    // Notify interested parties that the uniqueId has changed.
    DENG2_FOR_AUDIENCE(UniqueIdChanged, i) i->manifestUniqueIdChanged(*this);

    return true;
}

bool FontManifest::hasFont() const
{
    return !_font.isNull();
}

AbstractFont &FontManifest::font() const
{
    if(hasFont())
    {
        return *_font.data();
    }
    /// @throw MissingFontError There is no font associated with the manifest.
    throw MissingFontError("FontRecord::font", "No font is associated");
}

void FontManifest::setFont(AbstractFont *newFont)
{
    if(_font.data() != newFont)
    {
        if(AbstractFont *curFont = _font.data())
        {
            // Cancel notifications about the existing font.
            curFont->audienceForDeletion -= this;
        }

        _font.reset(newFont);

        if(AbstractFont *curFont = _font.data())
        {
            // We want notification when the new font is about to be deleted.
            curFont->audienceForDeletion += this;
        }
    }
}

void FontManifest::fontBeingDeleted(AbstractFont const & /*font*/)
{
    _font.reset();
}

} // namespace de
