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

using namespace de;

DENG2_PIMPL_NOREF(FontManifest)
{
    int uniqueId;
    QScopedPointer<AbstractFont>(resource); ///< Associated resource (if any).

    Instance() : uniqueId(0)
    {}
};

FontManifest::FontManifest(PathTree::NodeArgs const &args)
    : Node(args), d(new Instance())
{}

FontManifest::~FontManifest()
{
    DENG2_FOR_AUDIENCE(Deletion, i) i->manifestBeingDeleted(*this);
}

Fonts &FontManifest::collection()
{
    return App_Fonts();
}

FontScheme &FontManifest::scheme() const
{
    LOG_AS("FontManifest::scheme");
    /// @todo Optimize: FontManifest should contain a link to the owning FontScheme.
    foreach(FontScheme *scheme, collection().allSchemes())
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
    return d->uniqueId;
}

bool FontManifest::setUniqueId(int newUniqueId)
{
    if(d->uniqueId == newUniqueId) return false;

    d->uniqueId = newUniqueId;

    // Notify interested parties that the uniqueId has changed.
    DENG2_FOR_AUDIENCE(UniqueIdChanged, i) i->manifestUniqueIdChanged(*this);

    return true;
}

bool FontManifest::hasResource() const
{
    return !d->resource.isNull();
}

AbstractFont &FontManifest::resource() const
{
    if(hasResource())
    {
        return *d->resource.data();
    }
    /// @throw MissingFontError No resource is associated with the manifest.
    throw MissingFontError("FontManifest::font", "No resource is associated");
}

void FontManifest::setResource(AbstractFont *newResource)
{
    if(d->resource.data() != newResource)
    {
        if(AbstractFont *curFont = d->resource.data())
        {
            // Cancel notifications about the existing resource.
            curFont->audienceForDeletion -= this;
        }

        d->resource.reset(newResource);

        if(AbstractFont *curFont = d->resource.data())
        {
            // We want notification when the new resource is about to be deleted.
            curFont->audienceForDeletion += this;
        }
    }
}

void FontManifest::fontBeingDeleted(AbstractFont const & /*resource*/)
{
    d->resource.reset();
}
