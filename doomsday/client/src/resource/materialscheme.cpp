/** @file materialscheme.cpp Material system subspace scheme.
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

#include "MaterialManifest"

#include "resource/materialscheme.h"

using namespace de;

typedef PathTreeT<MaterialManifest> Index;

DENG2_PIMPL(MaterialScheme)
{
    /// Symbolic name of the scheme.
    String name;

    Instance(Public *i, String symbolicName) : Base(i), name(symbolicName)
    {}
};

MaterialScheme::MaterialScheme(String symbolicName)
    : Index(), d(new Instance(this, symbolicName))
{}

MaterialScheme::~MaterialScheme()
{
    clear();
    DENG2_ASSERT(Index::isEmpty());

    delete d;
}

String const &MaterialScheme::name() const
{
    return d->name;
}

void MaterialScheme::clear()
{
    Index::clear();
}

int MaterialScheme::size() const
{
    return Index::size();
}

MaterialManifest &MaterialScheme::declare(Path const &path)
{
    LOG_AS("MaterialScheme::declare");

    if(path.isEmpty())
    {
        /// @throw InvalidPathError An empty path was specified.
        throw InvalidPathError("MaterialScheme::declare", "Missing/zero-length path was supplied");
    }

    int const sizeBefore = size();
    Manifest *newManifest = &Index::insert(path);
    DENG2_ASSERT(newManifest);

    if(size() != sizeBefore)
    {
        // Notify interested parties that a new manifest was defined in the scheme.
        DENG2_FOR_AUDIENCE(ManifestDefined, i) i->schemeManifestDefined(*this, *newManifest);
    }

    return *newManifest;
}

bool MaterialScheme::has(Path const &path) const
{
    return Index::has(path, Index::NoBranch | Index::MatchFull);
}

MaterialManifest const &MaterialScheme::find(Path const &path) const
{
    return Index::find(path, Index::NoBranch | Index::MatchFull);
}

MaterialManifest &MaterialScheme::find(Path const &path)
{
    Index::Node const &found = const_cast<MaterialScheme const *>(this)->find(path);
    return const_cast<Index::Node &>(found);
}

MaterialScheme::Manifests const &MaterialScheme::manifests() const
{
    return Index::leafNodes();
}

#ifdef DENG2_DEBUG
void MaterialScheme::debugPrint() const
{
    Index::debugPrintHashDistribution();
    Index::debugPrint();
}
#endif
