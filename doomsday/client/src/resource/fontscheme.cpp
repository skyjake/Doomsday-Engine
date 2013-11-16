/** @file fontscheme.cpp Font resource scheme.
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

#include "resource/fontscheme.h"
#include <de/Log>
#include <QList>

using namespace de;

DENG2_PIMPL(FontScheme)
{
    String name; ///< Symbolic.
    Index index; ///< Mappings from paths to manifests.

    QList<Manifest *> uniqueIdLut; ///< Index with uniqueId - uniqueIdBase.
    bool uniqueIdLutDirty;
    int uniqueIdBase;

    Instance(Public *i)
        : Base(i)
        , uniqueIdLutDirty(false)
        , uniqueIdBase(0)
    {}

    ~Instance()
    {
        self.clear();
        DENG2_ASSERT(index.isEmpty()); // sanity check.
    }

    bool inline uniqueIdInLutRange(int uniqueId) const
    {
        return uniqueId - uniqueIdBase >= 0 &&
               (uniqueId - uniqueIdBase) < uniqueIdLut.size();
    }

    void findUniqueIdRange(int *minId, int *maxId)
    {
        if(!minId && !maxId) return;

        if(minId) *minId = DDMAXINT;
        if(maxId) *maxId = DDMININT;

        PathTreeIterator<Index> iter(index.leafNodes());
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
        /// @todo Only destroy the resource if this is the last remaining reference.
        manifest.clearResource();

        unlinkInUniqueIdLut(manifest);
    }

    /// @pre uniqueIdMap is large enough if initialized!
    void unlinkInUniqueIdLut(Manifest const &manifest)
    {
        DENG2_ASSERT(&manifest.scheme() == thisPublic); // sanity check.
        // If the lut is already considered 'dirty' do not unlink.
        if(!uniqueIdLutDirty)
        {
            int uniqueId = manifest.uniqueId();
            DENG2_ASSERT(uniqueIdInLutRange(uniqueId));
            uniqueIdLut[uniqueId - uniqueIdBase] = 0;
        }
    }

    /// @pre uniqueIdLut has been initialized and is large enough!
    void linkInUniqueIdLut(Manifest &manifest)
    {
        DENG2_ASSERT(&manifest.scheme() == thisPublic); // sanity check.
        int uniqueId = manifest.uniqueId();
        DENG_ASSERT(uniqueIdInLutRange(uniqueId));
        uniqueIdLut[uniqueId - uniqueIdBase] = &manifest;
    }

    void rebuildUniqueIdLut()
    {
        // Is a rebuild necessary?
        if(!uniqueIdLutDirty) return;

        // Determine the size of the LUT.
        int minId, maxId;
        findUniqueIdRange(&minId, &maxId);

        int lutSize = 0;
        if(minId > maxId) // None found?
        {
            uniqueIdBase = 0;
        }
        else
        {
            uniqueIdBase = minId;
            lutSize = maxId - minId + 1;
        }

        // Fill the LUT with initial values.
#ifdef DENG2_QT_4_7_OR_NEWER
        uniqueIdLut.reserve(lutSize);
#endif
        int i = 0;
        for(; i < uniqueIdLut.size(); ++i)
        {
            uniqueIdLut[i] = 0;
        }
        for(; i < lutSize; ++i)
        {
            uniqueIdLut.push_back(0);
        }

        if(lutSize)
        {
            // Populate the LUT.
            PathTreeIterator<Index> iter(index.leafNodes());
            while(iter.hasNext())
            {
                linkInUniqueIdLut(iter.next());
            }
        }

        uniqueIdLutDirty = false;
    }
};

FontScheme::FontScheme(String symbolicName) : d(new Instance(this))
{
    d->name = symbolicName;
}

void FontScheme::clear()
{
    /*PathTreeIterator<Index> iter(d->index.leafNodes());
    while(iter.hasNext())
    {
        d->deindex(iter.next());
    }*/
    d->index.clear();
    d->uniqueIdLutDirty = true;
}

String const &FontScheme::name() const
{
    return d->name;
}

FontScheme::Manifest &FontScheme::declare(Path const &path)
{
    LOG_AS("FontScheme::declare");

    if(path.isEmpty())
    {
        /// @throw InvalidPathError An empty path was specified.
        throw InvalidPathError("FontScheme::declare", "Missing/zero-length path was supplied");
    }

    int const sizeBefore = d->index.size();
    Manifest *newManifest = &d->index.insert(path);
    DENG2_ASSERT(newManifest != 0);

    if(d->index.size() != sizeBefore)
    {
        // We want notification if/when the manifest's uniqueId changes.
        newManifest->audienceForUniqueIdChange += this;

        // We want notification when the manifest is about to be deleted.
        newManifest->audienceForDeletion += this;

        // Notify interested parties that a new manifest was defined in the scheme.
        DENG2_FOR_AUDIENCE(ManifestDefined, i) i->schemeManifestDefined(*this, *newManifest);
    }

    return *newManifest;
}

bool FontScheme::has(Path const &path) const
{
    return d->index.has(path, Index::NoBranch | Index::MatchFull);
}

FontScheme::Manifest const &FontScheme::find(Path const &path) const
{
    if(has(path))
    {
        return d->index.find(path, Index::NoBranch | Index::MatchFull);
    }
    /// @throw NotFoundError Failed to locate a matching manifest.
    throw NotFoundError("FontScheme::find", "Failed to locate a manifest matching \"" + path.asText() + "\"");
}

FontScheme::Manifest &FontScheme::find(Path const &path)
{
    Manifest const &found = const_cast<FontScheme const *>(this)->find(path);
    return const_cast<Manifest &>(found);
}

FontScheme::Manifest const &FontScheme::findByUniqueId(int uniqueId) const
{
    d->rebuildUniqueIdLut();

    if(d->uniqueIdInLutRange(uniqueId))
    {
        Manifest *manifest = d->uniqueIdLut[uniqueId - d->uniqueIdBase];
        if(manifest) return *manifest;
    }
    /// @throw NotFoundError  No manifest was found with a matching resource URI.
    throw NotFoundError("FontScheme::findByUniqueId", "No manifest found with a unique ID matching \"" + QString("%1").arg(uniqueId) + "\"");
}

FontScheme::Manifest &FontScheme::findByUniqueId(int uniqueId)
{
    Manifest const &found = const_cast<FontScheme const *>(this)->findByUniqueId(uniqueId);
    return const_cast<Manifest &>(found);
}

FontScheme::Index const &FontScheme::index() const
{
    return d->index;
}

void FontScheme::manifestUniqueIdChanged(Manifest & /*manifest*/)
{
    // We'll need to rebuild the id map.
    d->uniqueIdLutDirty = true;
}

void FontScheme::manifestBeingDeleted(Manifest const &manifest)
{
    d->deindex(const_cast<Manifest &>(manifest));
}
