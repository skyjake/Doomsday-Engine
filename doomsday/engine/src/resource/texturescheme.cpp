/**
 * @file texturescheme.cpp Texture system subspace scheme.
 * @ingroup resource
 *
 * @author Copyright &copy; 2010-2012 Daniel Swanson <danij@dengine.net>
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

#include "resource/texturescheme.h"

namespace de {

struct TextureScheme::Instance
{
    /// Symbolic name of the scheme.
    String name;

    /// Mappings from paths to manifests.
    TextureScheme::Index *index_;

    /// LUT which translates scheme-unique-ids to their associated manifest (if any).
    /// Index with uniqueId - uniqueIdBase.
    QList<TextureManifest *> uniqueIdLut;
    bool uniqueIdLutDirty;
    int uniqueIdBase;

    Instance(String symbolicName)
        : name(symbolicName), index_(new TextureScheme::Index()),
          uniqueIdLut(), uniqueIdLutDirty(false), uniqueIdBase(0)
    {}

    ~Instance()
    {
        if(index_)
        {
            PathTreeIterator<PathTree> iter(index_->leafNodes());
            while(iter.hasNext())
            {
                TextureManifest &manifest = static_cast<TextureManifest &>(iter.next());
                deindex(manifest);
            }
            delete index_;
        }
    }

    bool inline uniqueIdInLutRange(int uniqueId) const
    {
        return (uniqueId - uniqueIdBase >= 0 && (uniqueId - uniqueIdBase) < uniqueIdLut.size());
    }

    void findUniqueIdRange(int *minId, int *maxId)
    {
        if(!minId && !maxId) return;

        if(!index_)
        {
            if(minId) *minId = 0;
            if(maxId) *maxId = 0;
            return;
        }

        if(minId) *minId = DDMAXINT;
        if(maxId) *maxId = DDMININT;

        PathTreeIterator<PathTree> iter(index_->leafNodes());
        while(iter.hasNext())
        {
            TextureManifest &manifest = static_cast<TextureManifest &>(iter.next());
            int const uniqueId = manifest.uniqueId();
            if(minId && uniqueId < *minId) *minId = uniqueId;
            if(maxId && uniqueId > *maxId) *maxId = uniqueId;
        }
    }

    void deindex(TextureManifest &manifest)
    {
        /// @todo Only destroy the texture if this is the last remaining reference.
        Texture *texture = manifest.texture();
        if(texture)
        {
            delete texture;
            manifest.setTexture(0);
        }

        unlinkInUniqueIdLut(manifest);
    }

    /// @pre uniqueIdLut is large enough if initialized!
    void unlinkInUniqueIdLut(TextureManifest &manifest)
    {
        // If the lut is already considered 'dirty' do not unlink.
        if(!uniqueIdLutDirty)
        {
            int uniqueId = manifest.uniqueId();
            DENG_ASSERT(uniqueIdInLutRange(uniqueId));
            uniqueIdLut[uniqueId - uniqueIdBase] = 0;
        }
    }

    /// @pre uniqueIdLut has been initialized and is large enough!
    void linkInUniqueIdLut(TextureManifest &manifest)
    {
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
        uniqueIdLut.reserve(lutSize);
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
            PathTreeIterator<PathTree> iter(index_->leafNodes());
            while(iter.hasNext())
            {
                linkInUniqueIdLut(static_cast<TextureManifest &>(iter.next()));
            }
        }

        uniqueIdLutDirty = false;
    }
};

TextureScheme::TextureScheme(String symbolicName)
{
    d = new Instance(symbolicName);
}

TextureScheme::~TextureScheme()
{
    delete d;
}

void TextureScheme::clear()
{
    if(d->index_)
    {
        PathTreeIterator<PathTree> iter(d->index_->leafNodes());
        while(iter.hasNext())
        {
            TextureManifest &manifest = static_cast<TextureManifest &>(iter.next());
            d->deindex(manifest);
        }
        d->index_->clear();
        d->uniqueIdLutDirty = true;
    }
}

String const &TextureScheme::name() const
{
    return d->name;
}

int TextureScheme::size() const
{
    return d->index_->size();
}

TextureManifest &TextureScheme::insertManifest(Path const &path)
{
    int sizeBefore = d->index_->size();
    TextureManifest &manifest = d->index_->insert(path);
    if(d->index_->size() != sizeBefore)
    {
        // We'll need to rebuild the unique id LUT after this.
        d->uniqueIdLutDirty = true;
    }
    return manifest;
}

TextureManifest const &TextureScheme::find(Path const &path) const
{
    try
    {
        return d->index_->find(path, PathTree::NoBranch | PathTree::MatchFull);
    }
    catch(Index::NotFoundError const &er)
    {
        throw NotFoundError("Textures::Scheme::find", er.asText());
    }
}

TextureManifest &TextureScheme::find(Path const &path)
{
    TextureManifest const &found = const_cast<TextureScheme const *>(this)->find(path);
    return const_cast<TextureManifest &>(found);
}

TextureManifest const &TextureScheme::findByResourceUri(Uri const &uri) const
{
    if(!uri.isEmpty())
    {
        PathTreeIterator<PathTree> iter(d->index_->leafNodes());
        while(iter.hasNext())
        {
            TextureManifest &manifest = static_cast<TextureManifest &>(iter.next());
            if(manifest.resourceUri() == uri)
            {
                return manifest;
            }
        }
    }
    /// @throw NotFoundError  No manifest was found with a matching resource URI.
    throw NotFoundError("Textures::Scheme::findByResourceUri", "No manifest found with a resource URI matching \"" + uri + "\"");
}

TextureManifest &TextureScheme::findByResourceUri(Uri const &uri)
{
    TextureManifest const &found = const_cast<TextureScheme const *>(this)->findByResourceUri(uri);
    return const_cast<TextureManifest &>(found);
}

TextureManifest const &TextureScheme::findByUniqueId(int uniqueId) const
{
    d->rebuildUniqueIdLut();

    if(d->uniqueIdInLutRange(uniqueId))
    {
        TextureManifest *manifest = d->uniqueIdLut[uniqueId - d->uniqueIdBase];
        if(manifest) return *manifest;
    }
    /// @throw NotFoundError  No manifest was found with a matching resource URI.
    throw NotFoundError("Textures::Scheme::findByUniqueId", "No manifest found with a unique ID matching \"" + QString("%1").arg(uniqueId) + "\"");
}

TextureManifest &TextureScheme::findByUniqueId(int uniqueId)
{
    TextureManifest const &found = const_cast<TextureScheme const *>(this)->findByUniqueId(uniqueId);
    return const_cast<TextureManifest &>(found);
}

TextureScheme::Index const &TextureScheme::index() const
{
    return *d->index_;
}

void TextureScheme::markUniqueIdLutDirty()
{
    d->uniqueIdLutDirty = true;
}

} // namespace de
