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

#include "de_platform.h"
#include "resource/fontmanifest.h"

#include "dd_main.h" // App_Fonts(), remove me
#include "FontScheme"
#include <de/Log>

using namespace de;

DENG2_PIMPL(FontManifest),
DENG2_OBSERVES(AbstractFont, Deletion)
{
    int uniqueId;
    QScopedPointer<AbstractFont>(resource); ///< Associated resource (if any).

    Instance(Public *i)
        : Base(i)
        , uniqueId(0)
    {}

    ~Instance()
    {
        DENG2_FOR_PUBLIC_AUDIENCE(Deletion, i) i->fontManifestBeingDeleted(self);
    }

    // Observes AbstractFont::Deletion.
    void fontBeingDeleted(AbstractFont const & /*resource*/)
    {
        resource.reset();
    }
};

FontManifest::FontManifest(PathTree::NodeArgs const &args)
    : Node(args), d(new Instance(this))
{}

FontScheme &FontManifest::scheme() const
{
    LOG_AS("FontManifest");
    /// @todo Optimize: FontManifest should contain a link to the owning FontScheme.
    foreach(FontScheme *scheme, App_ResourceSystem().allFontSchemes())
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
    LOG_AS("FontManifest");

    if(d->uniqueId == newUniqueId) return false;

    d->uniqueId = newUniqueId;

    // Notify interested parties that the uniqueId has changed.
    DENG2_FOR_AUDIENCE(UniqueIdChange, i) i->fontManifestUniqueIdChanged(*this);

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
    throw MissingFontError("FontManifest::resource", "No resource is associated");
}

void FontManifest::setResource(AbstractFont *newResource)
{
    LOG_AS("FontManifest");

    if(d->resource.data() != newResource)
    {
        if(AbstractFont *curFont = d->resource.data())
        {
            // Cancel notifications about the existing resource.
            curFont->audienceForDeletion -= d;
        }

        d->resource.reset(newResource);

        if(AbstractFont *curFont = d->resource.data())
        {
            // We want notification when the new resource is about to be deleted.
            curFont->audienceForDeletion += d;
        }
    }
}
