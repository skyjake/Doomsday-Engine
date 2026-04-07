/** @file fontmanifest.h Font resource manifest.
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

#ifndef DE_RESOURCE_FONTMANIFEST_H
#define DE_RESOURCE_FONTMANIFEST_H

#include "abstractfont.h"
#include <doomsday/uri.h>
#include <de/error.h>
#include <de/observers.h>
#include <de/pathtree.h>
#include <de/string.h>

namespace de {

class FontScheme;

/**
 * Description for a would-be logical Font resource.
 *
 * Models a reference to and the associated metadata for a logical font in the
 * font resource collection.
 *
 * @see FontScheme, AbstractFont
 * @ingroup resource
 */
class FontManifest : public PathTree::Node
{
public:
    /// Required Font instance is missing. @ingroup errors
    DE_ERROR(MissingFontError);

    /// Notified when the manifest is about to be deleted.
    DE_DEFINE_AUDIENCE(Deletion, void fontManifestBeingDeleted(const FontManifest &manifest))

    /// Notified whenever the unique identifier changes.
    DE_DEFINE_AUDIENCE(UniqueIdChange, void fontManifestUniqueIdChanged(FontManifest &manifest))

public:
    FontManifest(const PathTree::NodeArgs &args);

    /**
     * Returns the owning scheme of the manifest.
     */
    FontScheme &scheme() const;

    /**
     * Convenient method of returning the name of the owning scheme.
     *
     * @see scheme(), FontScheme::name()
     */
    const String &schemeName() const;

    /**
     * Compose a URI of the form "scheme:path" for the manifest.
     *
     * The scheme component of the URI will contain the symbolic name of
     * the scheme for the manifest.
     *
     * The path component of the URI will contain the percent-encoded path
     * of the manifest.
     */
    inline res::Uri composeUri(Char sep = '/') const {
        return res::Uri(schemeName(), path(sep));
    }

    /**
     * Compose a URN of the form "urn:scheme:uniqueid" for the manifest.
     *
     * The scheme component of the URI will contain the identifier 'urn'.
     *
     * The path component of the URI is a string which contains both the
     * symbolic name of the scheme followed by the unique id of the font
     * manifest, separated with a colon.
     *
     * @see uniqueId(), setUniqueId()
     */
    inline res::Uri composeUrn() const {
        return res::Uri("urn", Stringf("%s:%i", schemeName().c_str(), uniqueId()));
    }

    /**
     * Returns a textual description of the manifest.
     *
     * @return Human-friendly description the manifest.
     */
    String description(res::Uri::ComposeAsTextFlags uriCompositionFlags =
                           res::Uri::DefaultComposeAsTextFlags) const;

    /**
     * Returns the scheme-unique identifier for the manifest.
     */
    int uniqueId() const;

    /**
     * Change the unique identifier property of the manifest.
     *
     * @return  @c true iff @a newUniqueId differed to the existing unique
     *          identifier, which was subsequently changed.
     */
    bool setUniqueId(int newUniqueId);

    /**
     * Returns @c true if a resource is presently associated with the manifest.
     */
    bool hasResource() const;

    /**
     * Returns the logical resource associated with the manifest.
     */
    AbstractFont &resource() const;

    /**
     * Change the logical resource associated with the manifest.
     *
     * @param newResource  New resource to associate.
     */
    void setResource(AbstractFont *newResource);

    /**
     * Clear the logical resource associated with the manifest.
     *
     * Same as @c setResource(0)
     */
    inline void clearResource() { setResource(0); }

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // DE_RESOURCE_FONTMANIFEST_H
