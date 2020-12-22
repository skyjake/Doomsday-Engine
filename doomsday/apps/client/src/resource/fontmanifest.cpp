/** @file fontmanifest.cpp Font resource manifest.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "resource/fontscheme.h"

#include "dd_main.h" // App_Fonts(), remove me
#include <de/log.h>

using namespace de;

DE_PIMPL(FontManifest),
DE_OBSERVES(AbstractFont, Deletion)
{
    int uniqueId;
    std::unique_ptr<AbstractFont>(resource); ///< Associated resource (if any).

    Impl(Public *i)
        : Base(i)
        , uniqueId(0)
    {}

    ~Impl()
    {
        DE_NOTIFY_PUBLIC_VAR(Deletion, i) i->fontManifestBeingDeleted(self());
    }

    // Observes AbstractFont::Deletion.
    void fontBeingDeleted(const AbstractFont & /*resource*/)
    {
        resource.reset();
    }
};

FontManifest::FontManifest(const PathTree::NodeArgs &args)
    : Node(args), d(new Impl(this))
{}

FontScheme &FontManifest::scheme() const
{
    LOG_AS("FontManifest");
    /// @todo Optimize: FontManifest should contain a link to the owning FontScheme.
    for (const auto &scheme : App_Resources().allFontSchemes())
    {
        if (&scheme.second->index() == &tree()) return *scheme.second;
    }
    /// @throw Error Failed to determine the scheme of the manifest (should never happen...).
    throw Error("FontManifest::scheme", stringf("Failed to determine scheme for manifest [%p]", this));
}

const String &FontManifest::schemeName() const
{
    return scheme().name();
}

String FontManifest::description(res::Uri::ComposeAsTextFlags uriCompositionFlags) const
{
    return composeUri().compose(uriCompositionFlags | res::Uri::DecodePath);
//    return String("%1").arg(composeUri().compose(uriCompositionFlags | Uri::DecodePath),
//                            ( uriCompositionFlags.testFlag(Uri::OmitScheme)? -14 : -22 ) );
}

int FontManifest::uniqueId() const
{
    return d->uniqueId;
}

bool FontManifest::setUniqueId(int newUniqueId)
{
    LOG_AS("FontManifest");

    if (d->uniqueId == newUniqueId) return false;

    d->uniqueId = newUniqueId;

    // Notify interested parties that the uniqueId has changed.
    DE_NOTIFY_VAR(UniqueIdChange, i) i->fontManifestUniqueIdChanged(*this);

    return true;
}

bool FontManifest::hasResource() const
{
    return bool(d->resource);
}

AbstractFont &FontManifest::resource() const
{
    if (hasResource())
    {
        return *d->resource;
    }
    /// @throw MissingFontError No resource is associated with the manifest.
    throw MissingFontError("FontManifest::resource", "No resource is associated");
}

void FontManifest::setResource(AbstractFont *newResource)
{
    LOG_AS("FontManifest");

    if (d->resource.get() != newResource)
    {
        if (d->resource)
        {
            // Cancel notifications about the existing resource.
            d->resource->audienceForDeletion -= d;
        }

        d->resource.reset(newResource);

        if (d->resource)
        {
            // We want notification when the new resource is about to be deleted.
            d->resource->audienceForDeletion += d;
        }
    }
}
