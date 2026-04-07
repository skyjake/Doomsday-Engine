/** @file texturescheme.cpp  Texture system subspace scheme.
 *
 * @authors Copyright © 2010-2013 Daniel Swanson <danij@dengine.net>
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

#include "doomsday/res/texturescheme.h"
#include "doomsday/res/texturemanifest.h"

#include <de/legacy/types.h>

using namespace de;

namespace res {

DE_PIMPL(TextureScheme)
, DE_OBSERVES(TextureManifest, UniqueIdChange)
, DE_OBSERVES(TextureManifest, Deletion)
{
    /// Symbolic name of the scheme.
    String name;

    /// Mappings from paths to manifests.
    TextureScheme::Index index;

    /// LUT which translates scheme-unique-ids to their associated manifest (if any).
    /// Index with uniqueId - uniqueIdBase.
    List<TextureManifest *> uniqueIdLut;
    bool uniqueIdLutDirty;
    int uniqueIdBase;

    Impl(Public *i, const String &symbolicName) : Base(i),
        name(symbolicName),
        uniqueIdLut(),
        uniqueIdLutDirty(false),
        uniqueIdBase(0)
    {}

    ~Impl()
    {
        self().clear();
        DE_ASSERT(index.isEmpty());
    }

    bool inline uniqueIdInLutRange(int uniqueId) const
    {
        return (uniqueId - uniqueIdBase >= 0 && (uniqueId - uniqueIdBase) < uniqueIdLut.sizei());
    }

    void findUniqueIdRange(int *minId, int *maxId)
    {
        if (!minId && !maxId) return;

        if (minId) *minId = DDMAXINT;
        if (maxId) *maxId = DDMININT;

        PathTreeIterator<Index> iter(index.leafNodes());
        while (iter.hasNext())
        {
            TextureManifest &manifest = iter.next();
            const int uniqueId = manifest.uniqueId();
            if (minId && uniqueId < *minId) *minId = uniqueId;
            if (maxId && uniqueId > *maxId) *maxId = uniqueId;
        }
    }

    void deindex(TextureManifest &manifest)
    {
        /// @todo Only destroy the texture if this is the last remaining reference.
        manifest.clearTexture();

        unlinkInUniqueIdLut(manifest);
    }

    /// @pre uniqueIdLut is large enough if initialized!
    void unlinkInUniqueIdLut(TextureManifest &manifest)
    {
        // If the lut is already considered 'dirty' do not unlink.
        if (!uniqueIdLutDirty)
        {
            int uniqueId = manifest.uniqueId();
            DE_ASSERT(uniqueIdInLutRange(uniqueId));
            uniqueIdLut[uniqueId - uniqueIdBase] = 0;
        }
    }

    /// @pre uniqueIdLut has been initialized and is large enough!
    void linkInUniqueIdLut(TextureManifest &manifest)
    {
        int uniqueId = manifest.uniqueId();
        DE_ASSERT(uniqueIdInLutRange(uniqueId));
        uniqueIdLut[uniqueId - uniqueIdBase] = &manifest;
    }

    void rebuildUniqueIdLut()
    {
        // Is a rebuild necessary?
        if (!uniqueIdLutDirty) return;

        // Determine the size of the LUT.
        int minId, maxId;
        findUniqueIdRange(&minId, &maxId);

        int lutSize = 0;
        if (minId > maxId) // None found?
        {
            uniqueIdBase = 0;
        }
        else
        {
            uniqueIdBase = minId;
            lutSize = maxId - minId + 1;
        }

        // Fill the LUT with initial values.
        uniqueIdLut.reserve(lutSize);
        int i = 0;
        for (; i < uniqueIdLut.sizei(); ++i)
        {
            uniqueIdLut[i] = 0;
        }
        for (; i < lutSize; ++i)
        {
            uniqueIdLut.push_back(0);
        }

        if (lutSize)
        {
            // Populate the LUT.
            PathTreeIterator<Index> iter(index.leafNodes());
            while (iter.hasNext())
            {
                linkInUniqueIdLut(iter.next());
            }
        }

        uniqueIdLutDirty = false;
    }

    // Observes TextureManifest UniqueIdChange
    void textureManifestUniqueIdChanged(Manifest & /*manifest*/)
    {
        // We'll need to rebuild the id map.
        uniqueIdLutDirty = true;
    }

    // Observes TextureManifest Deletion.
    void textureManifestBeingDeleted(const TextureManifest &manifest)
    {
        deindex(const_cast<TextureManifest &>(manifest));
    }
};

TextureScheme::TextureScheme(const String& symbolicName)
    : d(new Impl(this, symbolicName))
{}

TextureScheme::~TextureScheme()
{
    clear();
}

void TextureScheme::clear()
{
    /*PathTreeIterator<Index> iter(d->index.leafNodes());
    while (iter.hasNext())
    {
        d->deindex(iter.next());
    }*/
    d->index.clear();
    d->uniqueIdLutDirty = true;
}

const String &TextureScheme::name() const
{
    return d->name;
}

TextureManifest &TextureScheme::declare(const Path &   path,
                                        Flags          flags,
                                        const Vec2ui & dimensions,
                                        const Vec2i &  origin,
                                        int            uniqueId,
                                        const res::Uri *resourceUri)
{
    LOG_AS("TextureScheme::declare");

    if (path.isEmpty())
    {
        /// @throw InvalidPathError An empty path was specified.
        throw InvalidPathError("TextureScheme::declare", "Missing/zero-length path was supplied");
    }

    const int sizeBefore = d->index.size();
    Manifest *newManifest = &d->index.insert(path);
    DE_ASSERT(newManifest);

    if (d->index.size() != sizeBefore)
    {
        // We'll need to rebuild the unique id LUT after this (deferred for perf).
        d->uniqueIdLutDirty = true;

        // We want notification if/when the manifest's uniqueId changes.
        newManifest->audienceForUniqueIdChange += d;

        // We want notification when the manifest is about to be deleted.
        newManifest->audienceForDeletion += d;

        // Notify interested parties that a new manifest was defined in the scheme.
        DE_NOTIFY_VAR(ManifestDefined, i) i->textureSchemeManifestDefined(*this, *newManifest);
    }

    /*
     * (Re)configure the manifest.
     */
    bool mustRelease = false;

    newManifest->setScheme(*this);
    newManifest->setFlags(flags);
    newManifest->setOrigin(origin);

    if (newManifest->setLogicalDimensions(dimensions))
    {
        mustRelease = true;
    }

    // We don't care whether these identfiers are truely unique. Our only
    // responsibility is to release textures when they change.
    if (newManifest->setUniqueId(uniqueId))
    {
        mustRelease = true;
    }

    if (resourceUri && newManifest->setResourceUri(*resourceUri))
    {
        // The mapped resource is being replaced, so release any existing Texture.
        /// @todo Only release if this Texture is bound to only this binding.
        mustRelease = true;
    }

    if (mustRelease && newManifest->hasTexture())
    {
        /// @todo Update any Materials (and thus Surfaces) which reference this.
        newManifest->texture().release();
    }

    return *newManifest;
}

#if 0
bool TextureScheme::has(const Path &path) const
{
    return d->index.has(path, Index::NoBranch | Index::MatchFull);
}
#endif

const TextureManifest &TextureScheme::find(const Path &path) const
{
    if (auto *mft = tryFind(path))
    {
        return *mft;
    }
    /// @throw NotFoundError Failed to locate a matching manifest.
    throw NotFoundError("TextureScheme::find", "Failed to locate a manifest matching \"" +
                        path.asText() + "\"");

}

TextureManifest &TextureScheme::find(const Path &path)
{
    const TextureManifest &found = const_cast<const TextureScheme *>(this)->find(path);
    return const_cast<TextureManifest &>(found);
}

TextureManifest *TextureScheme::tryFind(const Path &path) const
{
    return d->index.tryFind(path, Index::NoBranch | Index::MatchFull);
}

const TextureManifest &TextureScheme::findByResourceUri(const res::Uri &uri) const
{
    if (auto *mft = tryFindByResourceUri(uri))
    {
        return *mft;
    }
    /// @throw NotFoundError  No manifest was found with a matching resource URI.
    throw NotFoundError("TextureScheme::findByResourceUri",
                        "No manifest found with a resource URI matching \"" + uri.asText() + "\"");
}

TextureManifest *TextureScheme::tryFindByResourceUri(const res::Uri &uri) const
{
    if (!uri.isEmpty())
    {
        PathTreeIterator<Index> iter(d->index.leafNodes());
        while (iter.hasNext())
        {
            TextureManifest &manifest = iter.next();
            if (manifest.hasResourceUri())
            {
                if (manifest.resourceUri() == uri)
                {
                    return &manifest;
                }
            }
        }
    }
    return nullptr;
}

TextureManifest &TextureScheme::findByResourceUri(const res::Uri &uri)
{
    const TextureManifest &found = const_cast<const TextureScheme *>(this)->findByResourceUri(uri);
    return const_cast<TextureManifest &>(found);
}

const TextureManifest &TextureScheme::findByUniqueId(int uniqueId) const
{
    if (auto *mft = tryFindByUniqueId(uniqueId))
    {
        return *mft;
    }
    /// @throw NotFoundError  No manifest was found with a matching resource URI.
    throw NotFoundError("TextureScheme::findByUniqueId",
                        "No manifest found with a unique ID matching \"" + String::asText(uniqueId) + "\"");
}

TextureManifest &TextureScheme::findByUniqueId(int uniqueId)
{
    const TextureManifest &found = const_cast<const TextureScheme *>(this)->findByUniqueId(uniqueId);
    return const_cast<TextureManifest &>(found);
}

TextureManifest *TextureScheme::tryFindByUniqueId(int uniqueId) const
{
    d->rebuildUniqueIdLut();

    if (d->uniqueIdInLutRange(uniqueId))
    {
        TextureManifest *manifest = d->uniqueIdLut[uniqueId - d->uniqueIdBase];
        if (manifest) return manifest;
    }
    return nullptr;
}

const TextureScheme::Index &TextureScheme::index() const
{
    return d->index;
}

} // namespace res
