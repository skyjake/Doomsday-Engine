/** @file fontscheme.h Font resource scheme.
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

#ifndef DENG_RESOURCE_FONTSCHEME_H
#define DENG_RESOURCE_FONTSCHEME_H

#include "FontManifest"
#include "uri.hh"
#include <de/Error>
#include <de/Observers>
#include <de/PathTree>
#include <de/String>
#include <QList>

namespace de {

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
            _uniqueIdLut[uniqueId - _uniqueIdBase] = 0;
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

} // namespace de

#endif // DENG_RESOURCE_FONTSCHEME_H
