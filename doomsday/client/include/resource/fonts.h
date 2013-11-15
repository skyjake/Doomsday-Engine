/** @file fonts.h Font resource collection.
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

#ifndef DENG_RESOURCE_FONTS_H
#define DENG_RESOURCE_FONTS_H

#include "AbstractFont"
#include "FontManifest"
#include "FontScheme"
#include "uri.hh"
#include <de/Error>
#include <de/Observers>
#include <de/String>
#include <QList>
#include <QMap>

/// Special value used to signify an invalid font id.
#define NOFONTID                    0

namespace de {

/**
 * Font resource collection.
 *
 * There are two general states for a font:
 *
 *   A) Declared but not defined.
 *   B) Declared and defined.
 *
 * @ingroup resource
 */
class Fonts :
DENG2_OBSERVES(FontScheme, ManifestDefined),
DENG2_OBSERVES(FontManifest, Deletion),
DENG2_OBSERVES(AbstractFont, Deletion)
{
    /// Internal typedefs for brevity/cleanliness.
    typedef class FontManifest Manifest;
    typedef class FontScheme Scheme;

public:
    /// The referenced font/manifest was not found. @ingroup errors
    DENG2_ERROR(NotFoundError);

    /// An unknown scheme was referenced. @ingroup errors
    DENG2_ERROR(UnknownSchemeError);

    /// The specified font id was invalid (out of range). @ingroup errors
    DENG2_ERROR(UnknownIdError);

    typedef QMap<String, Scheme *> Schemes;
    typedef QList<AbstractFont *> All;

public:
    /**
     * Constructs a new font resource collection.
     */
    Fonts();

    /**
     * Register the console commands, variables, etc..., of this module.
     */
    static void consoleRegister();

    /**
     * Returns the total number of unique fonts in the collection.
     */
    uint count() const { return all().count(); }

    /**
     * Returns the total number of unique fonts in the collection.
     *
     * Same as size()
     */
    inline uint size() const { return count(); }

    /**
     * Determines if a manifest exists for a declared font on @a path.
     * @return @c true, if a manifest exists; otherwise @a false.
     */
    bool has(Uri const &path) const;

    /**
     * Find the manifest for a declared font.
     *
     * @param search  The search term.
     * @return Found unique identifier.
     */
    Manifest &find(Uri const &search) const;

    /**
     * Lookup a manifest by unique identifier.
     *
     * @param id  Unique identifier for the manifest to be looked up. Note
     *            that @c 0 is not a valid identifier.
     *
     * @return  The associated manifest.
     */
    Manifest &toManifest(fontid_t id) const;

    /**
     * Lookup a subspace scheme by symbolic name.
     *
     * @param name  Symbolic name of the scheme.
     * @return  Scheme associated with @a name.
     *
     * @throws UnknownSchemeError If @a name is unknown.
     */
    Scheme &scheme(String name) const;

    /**
     * Create a new subspace scheme.
     *
     * @note Scheme creation order defines the order in which schemes are tried
     *       by @ref find() when presented with an ambiguous URI (i.e., those
     *       without a scheme).
     *
     * @param name      Unique symbolic name of the new scheme. Must be at
     *                  least @c Scheme::min_name_length characters long.
     */
    Scheme &createScheme(String name);

    /**
     * Returns @c true iff a Scheme exists with the symbolic @a name.
     */
    bool knownScheme(String name) const;

    /**
     * Returns a list of all the schemes for efficient traversal.
     */
    Schemes const &allSchemes() const;

    /**
     * Returns the total number of manifest schemes in the collection.
     */
    inline int schemeCount() const { return allSchemes().count(); }

    /**
     * Clear all fonts in all schemes.
     *
     * @see allSchemes(), Scheme::clear().
     */
    inline void clearAllSchemes()
    {
        foreach(Scheme *scheme, allSchemes())
        {
            scheme->clear();
        }
    }

    /**
     * Declare a font in the collection, producing a manifest for a logical
     * AbstractFont which will be defined later. If a manifest with the specified
     * @a uri already exists the existing manifest will be returned.
     *
     * @param uri  Uri representing a path to the font in the virtual hierarchy.
     *
     * @return  Manifest for this URI.
     */
    inline Manifest &declare(Uri const &uri)
    {
        return scheme(uri.scheme()).declare(uri.path());
    }

    /**
     * Returns a list of all the unique texture instances in the collection,
     * from all schemes.
     */
    All const &all() const;

protected:
    // Observes Scheme ManifestDefined.
    void schemeManifestDefined(Scheme &scheme, Manifest &manifest);

    // Observes Manifest Deletion.
    void manifestBeingDeleted(Manifest const &manifest);

    // Observes Manifest FontDerived.
    //void manifestFontDerived(Manifest &manifest, AbstractFont &font);

    // Observes AbstractFont Deletion.
    void fontBeingDeleted(AbstractFont const &font);

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // DENG_RESOURCE_FONTS_H
