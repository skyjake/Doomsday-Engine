/** @file lumpcatalog.cpp
 *
 * @authors Copyright (c) 2016 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "doomsday/resource/lumpcatalog.h"
#include "doomsday/resource/lumpdirectory.h"
#include "doomsday/resource/databundle.h"

#include <de/App>
#include <de/LinkFile>
#include <de/PackageLoader>
#include <utility>

using namespace de;

namespace res {

DENG2_PIMPL(LumpCatalog)
{
    using Found = std::pair<DataBundle const *, LumpDirectory::Pos>;

    StringList packageIds;
    QList<DataBundle const *> bundles; /// @todo Should observe for deletion. -jk

    Instance(Public *i) : Base(i) {}

    void clear()
    {
        packageIds.clear();
        bundles.clear();
    }

    void updateBundles()
    {
        bundles.clear();

        for (auto const &pkg : packageIds)
        {
            // The package must be available as a file.
            if (File const *file = App::packageLoader().select(pkg))
            {
                auto const *bundle = file->target().maybeAs<DataBundle>();
                if (bundle && bundle->lumpDirectory())
                {
                    bundles << bundle;
                }
            }
        }
    }

    Found findLump(String const &name) const
    {
        Block const lumpName = name.toLatin1();
        Found found { nullptr, LumpDirectory::InvalidPos };

        // The last bundle is checked first.
        for (int i = bundles.size() - 1; i >= 0; --i)
        {
            auto const pos = bundles.at(i)->lumpDirectory()->find(lumpName);
            if (pos != LumpDirectory::InvalidPos)
            {
                found = Found(bundles.at(i), pos);
                break;
            }
        }
        return found;
    }
};

LumpCatalog::LumpCatalog()
    : d(new Instance(this))
{}

void LumpCatalog::clear()
{
    d->clear();
}

bool LumpCatalog::setPackages(StringList const &packageIds)
{
    if (packageIds != d->packageIds)
    {
        d->packageIds = packageIds;
        d->updateBundles();
        return true;
    }
    return false;
}

Block LumpCatalog::read(String const &lumpName) const
{
    Block data;
    Instance::Found found = d->findLump(lumpName);
    if (found.first)
    {
        auto const &entry = found.first->lumpDirectory()->entry(found.second);
        data.copyFrom(*found.first, entry.offset, entry.size);
    }
    return data;
}

} // namespace res
