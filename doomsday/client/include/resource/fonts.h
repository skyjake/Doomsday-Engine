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
#include "uri.hh"
#include <de/Error>
#include <de/PathTree>
#include <de/String>
#include <QMap>

/// Special value used to signify an invalid font id.
#define NOFONTID                    0

namespace de {

class Fonts;
class FontScheme;

/**
 * FontManifest. Stores metadata for a unique Font in the collection.
 */
class FontManifest : public PathTree::Node,
DENG2_OBSERVES(AbstractFont, Deletion)
{
public:
    /// Required Font instance is missing. @ingroup errors
    DENG2_ERROR(MissingFontError);

    DENG2_DEFINE_AUDIENCE(Deletion, void manifestBeingDeleted(FontManifest const &manifest))
    DENG2_DEFINE_AUDIENCE(UniqueIdChanged, void manifestUniqueIdChanged(FontManifest &manifest))

public:
    /// Scheme-unique identifier chosen by the owner of the collection.
    int _uniqueId;

    /// The defined font instance (if any).
    QScopedPointer<AbstractFont>(_font);

public:
    FontManifest(PathTree::NodeArgs const &args);
    ~FontManifest();

    /**
     * Returns the owning scheme of the manifest.
     */
    FontScheme &scheme() const;

    /// Convenience method for returning the name of the owning scheme.
    String const &schemeName() const;

    /**
     * Compose a URI of the form "scheme:path" for the FontRecord.
     *
     * The scheme component of the URI will contain the symbolic name of
     * the scheme for the FontRecord.
     *
     * The path component of the URI will contain the percent-encoded path
     * of the FontRecord.
     */
    inline Uri composeUri(QChar sep = '/') const
    {
        return Uri(schemeName(), path(sep));
    }

    /**
     * Compose a URN of the form "urn:scheme:uniqueid" for the font
     * FontRecord.
     *
     * The scheme component of the URI will contain the identifier 'urn'.
     *
     * The path component of the URI is a string which contains both the
     * symbolic name of the scheme followed by the unique id of the font
     * FontRecord, separated with a colon.
     *
     * @see uniqueId(), setUniqueId()
     */
    inline Uri composeUrn() const
    {
        return Uri("urn", String("%1:%2").arg(schemeName()).arg(uniqueId(), 0, 10));
    }

    /**
     * Returns a textual description of the manifest.
     *
     * @return Human-friendly description the manifest.
     */
    String description(Uri::ComposeAsTextFlags uriCompositionFlags = Uri::DefaultComposeAsTextFlags) const;

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
     * Returns @c true if a Font is presently associated with the manifest.
     */
    bool hasFont() const;

    /**
     * Returns the logical Font associated with the manifest.
     */
    AbstractFont &font() const;

    /**
     * Change the logical Font associated with the manifest.
     *
     * @param newFont  New logical Font to associate.
     */
    void setFont(AbstractFont *newFont);

    /**
     * Clear the logical Font associated with the manifest.
     *
     * Same as @c setFont(0)
     */
    inline void clearFont() { setFont(0); }

    /// Returns a reference to the application's font collection.
    static Fonts &fonts();

protected:
    // Observes AbstractFont::Deletion.
    void fontBeingDeleted(AbstractFont const &font);
};

class FontScheme :
DENG2_OBSERVES(FontManifest, UniqueIdChanged),
DENG2_OBSERVES(FontManifest, Deletion)
{
    typedef class FontManifest Manifest;

public:
    /// The requested manifests could not be found in the index.
    DENG2_ERROR(NotFoundError);

    /// The specified path was not valid. @ingroup errors
    DENG2_ERROR(InvalidPathError);

    DENG2_DEFINE_AUDIENCE(ManifestDefined, void schemeManifestDefined(FontScheme &scheme, Manifest &manifest))

    /// Minimum length of a symbolic name.
    static int const min_name_length = DENG2_URI_MIN_SCHEME_LENGTH;

    /// Manifests in the scheme are placed into a tree.
    typedef PathTreeT<Manifest> Index;

public: /// @todo make private:
    /// Symbolic name of the scheme.
    String _name;

    /// Mappings from paths to manifests.
    Index _index;

    /// LUT which translates scheme-unique-ids to their associated manifest (if any).
    /// Index with uniqueId - uniqueIdBase.
    QList<Manifest *> _uniqueIdLut;
    bool _uniqueIdLutDirty;
    int _uniqueIdBase;

public:
    /**
     * Construct a new (empty) texture subspace scheme.
     *
     * @param symbolicName  Symbolic name of the new subspace scheme. Must
     *                      have at least @ref min_name_length characters.
     */
    FontScheme(String symbolicName);
    ~FontScheme();

    /// @return  Symbolic name of this scheme (e.g., "System").
    String const &name() const;

    /// @return  Total number of records in the scheme.
    inline int size() const { return index().size(); }

    /// @return  Total number of records in the scheme. Same as @ref size().
    inline int count() const { return size(); }

    /**
     * Clear all records in the scheme (any GL textures which have been
     * acquired for associated font textures will be released).
     */
    void clear();

    /**
     * Insert a new manifest at the given @a path into the scheme.
     * If a manifest already exists at this path, the existing manifest is
     * returned and the call is a no-op.
     *
     * @param path  Virtual path for the resultant manifest.
     * @return  The (possibly newly created) manifest at @a path.
     */
    Manifest &declare(Path const &path);

    /**
     * Determines if a manifest exists on the given @a path.
     * @return @c true if a manifest exists; otherwise @a false.
     */
    bool has(Path const &path) const;

    /**
     * Search the scheme for a manifest matching @a path.
     *
     * @return  Found manifest.
     */
    Manifest const &find(Path const &path) const;

    /// @copydoc find()
    Manifest &find(Path const &path);

    /**
     * Search the scheme for a manifest whose associated unique
     * identifier matches @a uniqueId.
     *
     * @return  Found manifest.
     */
    Manifest const &findByUniqueId(int uniqueId) const;

    /// @copydoc findByUniqueId()
    Manifest &findByUniqueId(int uniqueId);

    /**
     * Provides access to the manifest index for efficient traversal.
     */
    Index const &index() const;

protected:
    // Observes Manifest UniqueIdChanged
    void manifestUniqueIdChanged(Manifest &manifest);

    // Observes Manifest Deletion.
    void manifestBeingDeleted(Manifest const &manifest);

private:
    bool inline uniqueIdInLutRange(int uniqueId) const
    {
        return (uniqueId - _uniqueIdBase >= 0 && (uniqueId - _uniqueIdBase) < _uniqueIdLut.size());
    }

    void findUniqueIdRange(int *minId, int *maxId)
    {
        if(!minId && !maxId) return;

        if(minId) *minId = DDMAXINT;
        if(maxId) *maxId = DDMININT;

        PathTreeIterator<Index> iter(_index.leafNodes());
        while(iter.hasNext())
        {
            Manifest &manifest = iter.next();
            int const uniqueId = manifest.uniqueId();
            if(minId && uniqueId < *minId) *minId = uniqueId;
            if(maxId && uniqueId > *maxId) *maxId = uniqueId;
        }
    }

    void deindex(Manifest &manifest)
    {
        /// @todo Only destroy the font if this is the last remaining reference.
        manifest.clearFont();

        unlinkInUniqueIdLut(manifest);
    }

    /// @pre uniqueIdMap is large enough if initialized!
    void unlinkInUniqueIdLut(Manifest const &manifest)
    {
        DENG2_ASSERT(&manifest.scheme() == this); // sanity check.
        // If the lut is already considered 'dirty' do not unlink.
        if(!_uniqueIdLutDirty)
        {
            int uniqueId = manifest.uniqueId();
            DENG2_ASSERT(uniqueIdInLutRange(uniqueId));
            _uniqueIdLut[uniqueId - _uniqueIdBase] = NOFONTID;
        }
    }

    /// @pre uniqueIdLut has been initialized and is large enough!
    void linkInUniqueIdLut(Manifest &manifest)
    {
        DENG2_ASSERT(&manifest.scheme() == this); // sanity check.
        int uniqueId = manifest.uniqueId();
        DENG_ASSERT(uniqueIdInLutRange(uniqueId));
        _uniqueIdLut[uniqueId - _uniqueIdBase] = &manifest;
    }

    void rebuildUniqueIdLut()
    {
        // Is a rebuild necessary?
        if(!_uniqueIdLutDirty) return;

        // Determine the size of the LUT.
        int minId, maxId;
        findUniqueIdRange(&minId, &maxId);

        int lutSize = 0;
        if(minId > maxId) // None found?
        {
            _uniqueIdBase = 0;
        }
        else
        {
            _uniqueIdBase = minId;
            lutSize = maxId - minId + 1;
        }

        // Fill the LUT with initial values.
#ifdef DENG2_QT_4_7_OR_NEWER
        _uniqueIdLut.reserve(lutSize);
#endif
        int i = 0;
        for(; i < _uniqueIdLut.size(); ++i)
        {
            _uniqueIdLut[i] = 0;
        }
        for(; i < lutSize; ++i)
        {
            _uniqueIdLut.push_back(0);
        }

        if(lutSize)
        {
            // Populate the LUT.
            PathTreeIterator<Index> iter(_index.leafNodes());
            while(iter.hasNext())
            {
                linkInUniqueIdLut(iter.next());
            }
        }

        _uniqueIdLutDirty = false;
    }
};

/**
 * Font resource collection.
 *
 * @em Runtime fonts are not loaded until precached or actually needed. They may
 * be cleared, in which case they will be reloaded when needed.
 *
 * @em System fonts are loaded at startup and remain in memory all the time. After
 * clearing they must be manually reloaded.
 *
 * "Clearing" a font means any names bound to it are deleted and any GL textures
 * acquired for it are 'released' at this time). The Font instance record used
 * to represent it is also deleted.
 *
 * "Releasing" a font will release any GL textures acquired for it.
 *
 * Thus there are two general states for a font:
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
     * To be called during a definition database reset to clear all links to defs.
     */
    void clearDefinitionLinks();

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
