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

namespace de {

FontScheme::FontScheme(String symbolicName)
    : _name(symbolicName)
    , _uniqueIdLutDirty(false)
    , _uniqueIdBase(0)
{}

FontScheme::~FontScheme()
{
    clear();
    DENG2_ASSERT(_index.isEmpty()); // sanity check.
}

void FontScheme::clear()
{
    /*PathTreeIterator<Index> iter(_index.leafNodes());
    while(iter.hasNext())
    {
        deindex(iter.next());
    }*/
    _index.clear();
    _uniqueIdLutDirty = true;
}

String const &FontScheme::name() const
{
    return _name;
}

FontScheme::Manifest &FontScheme::declare(Path const &path)
{
    LOG_AS("FontScheme::declare");

    if(path.isEmpty())
    {
        /// @throw InvalidPathError An empty path was specified.
        throw InvalidPathError("FontScheme::declare", "Missing/zero-length path was supplied");
    }

    int const sizeBefore = _index.size();
    Manifest *newManifest = &_index.insert(path);
    DENG2_ASSERT(newManifest);

    if(_index.size() != sizeBefore)
    {
        // We want notification if/when the manifest's uniqueId changes.
        newManifest->audienceForUniqueIdChanged += this;

        // We want notification when the manifest is about to be deleted.
        newManifest->audienceForDeletion += this;

        // Notify interested parties that a new manifest was defined in the scheme.
        DENG2_FOR_AUDIENCE(ManifestDefined, i) i->schemeManifestDefined(*this, *newManifest);
    }

    return *newManifest;
}

bool FontScheme::has(Path const &path) const
{
    return _index.has(path, Index::NoBranch | Index::MatchFull);
}

FontScheme::Manifest const &FontScheme::find(Path const &path) const
{
    if(has(path))
    {
        return _index.find(path, Index::NoBranch | Index::MatchFull);
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
    const_cast<FontScheme *>(this)->rebuildUniqueIdLut();

    if(uniqueIdInLutRange(uniqueId))
    {
        Manifest *manifest = _uniqueIdLut[uniqueId - _uniqueIdBase];
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
    return _index;
}

void FontScheme::manifestUniqueIdChanged(Manifest & /*manifest*/)
{
    // We'll need to rebuild the id map.
    _uniqueIdLutDirty = true;
}

void FontScheme::manifestBeingDeleted(Manifest const &manifest)
{
    deindex(const_cast<Manifest &>(manifest));
}

} // namespace de
