/** @file materialscheme.cpp  Material system subspace scheme.
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

#include "doomsday/world/materialscheme.h"
#include "doomsday/world/materialmanifest.h"

using namespace de;

namespace world {

DE_PIMPL(MaterialScheme)
{
    /// Symbolic name of the scheme.
    String name;

    /// Mappings from paths to manifests.
    MaterialScheme::Index index;

    Impl(Public *i, String symbolicName) : Base(i), name(symbolicName)
    {}

    ~Impl()
    {
        self().clear();
        DE_ASSERT(index.isEmpty());
    }
};

MaterialScheme::MaterialScheme(String symbolicName)
    : d(new Impl(this, symbolicName))
{}

void MaterialScheme::clear()
{
    d->index.clear();
}

const String &MaterialScheme::name() const
{
    return d->name;
}

MaterialManifest &MaterialScheme::declare(const Path &path)
{
    LOG_AS("MaterialScheme::declare");

    if (path.isEmpty())
    {
        /// @throw InvalidPathError An empty path was specified.
        throw InvalidPathError("MaterialScheme::declare", "Missing/zero-length path was supplied");
    }

    const int sizeBefore = d->index.size();
    Manifest *newManifest = &d->index.insert(path);
    DE_ASSERT(newManifest);

    newManifest->setScheme(*this);

    if (d->index.size() != sizeBefore)
    {
        // Notify interested parties that a new manifest was defined in the scheme.
        DE_NOTIFY_VAR(ManifestDefined, i) i->materialSchemeManifestDefined(*this, *newManifest);
    }

    return *newManifest;
}

bool MaterialScheme::has(const Path &path) const
{
    return d->index.has(path, Index::NoBranch | Index::MatchFull);
}

const MaterialManifest &MaterialScheme::find(const Path &path) const
{
    if (has(path))
    {
        return d->index.find(path, Index::NoBranch | Index::MatchFull);
    }
    /// @throw NotFoundError Failed to locate a matching manifest.
    throw NotFoundError("MaterialScheme::find", "Failed to locate a manifest matching \"" + path.asText() + "\"");
}

MaterialManifest &MaterialScheme::find(const Path &path)
{
    const Index::Node &found = const_cast<const MaterialScheme *>(this)->find(path);
    return const_cast<Index::Node &>(found);
}

MaterialManifest *MaterialScheme::tryFind(const Path & path) const
{
    return d->index.tryFind(path, Index::NoBranch | Index::MatchFull);
}

const MaterialScheme::Index &MaterialScheme::index() const
{
    return d->index;
}

} // namespace world
