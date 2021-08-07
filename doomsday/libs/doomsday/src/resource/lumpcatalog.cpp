/** @file lumpcatalog.cpp
 *
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "doomsday/res/lumpcatalog.h"
#include "doomsday/res/lumpdirectory.h"
#include "doomsday/res/databundle.h"

#include <de/app.h>
#include <de/linkfile.h>
#include <de/packageloader.h>
#include <utility>

using namespace de;

namespace res {

DE_PIMPL(LumpCatalog)
{
    StringList packageIds;
    List<const DataBundle *> bundles; /// @todo Should observe for deletion. -jk

    Impl(Public *i)
        : Base(i)
    {}
    
    Impl(Public *i, const Impl &other)
        : Base(i)
        , packageIds(other.packageIds)
        , bundles(other.bundles)
    {}
    
    ~Impl()
    {
        for (const auto *bundle : bundles)
        {
            bundle->asFile().release();
        }
    }

    void clear()
    {
        packageIds.clear();
        bundles.clear();
    }
    
    void updateBundles()
    {
        bundles.clear();
        
        for (const auto &pkg : packageIds)
        {
            // The package must be available as a file.
            if (const File *file = App::packageLoader().select(pkg))
            {
                const auto *bundle = maybeAs<DataBundle>(file->target());
                if (bundle && bundle->lumpDirectory())
                {
                    bundles << bundle;
                }
            }
        }
    }

    LumpPos findLump(const String &name) const
    {
        LumpPos found{nullptr, LumpDirectory::InvalidPos};

        // The last bundle is checked first.
        for (auto i = bundles.rbegin(); i != bundles.rend(); ++i)
        {
            const auto pos = (*i)->lumpDirectory()->find(name);
            if (pos != LumpDirectory::InvalidPos)
            {
                found = {*i, pos};
                break;
            }    
        }
        return found;
    }

    List<LumpPos> findAllLumps(const String &name) const
    {
        List<LumpPos> found;
//        const Block lumpName = name;
        for (auto i = bundles.rbegin(); i != bundles.rend(); ++i)
        {
            const auto *bundle = *i;
            found += map<List<LumpPos>>(bundle->lumpDirectory()->findAll(name),
                                         [bundle](dsize pos) -> LumpPos {
                                             return {bundle, pos};
                                         });
        }
        return found;
    }

    List<LumpRange> flatRanges() const
    {
        List<LumpRange> ranges;
        for (auto i = bundles.rbegin(); i != bundles.rend(); ++i)
        {
            const auto flats = (*i)->lumpDirectory()->findRanges(LumpDirectory::Flats);
            for (const auto &r : flats) ranges.push_back({*i, r});
    }
        return ranges;
    }
};

LumpCatalog::LumpCatalog()
    : d(new Impl(this))
{}
    
LumpCatalog::LumpCatalog(const LumpCatalog &other)
    : d(new Impl(this, *other.d))
{}

void LumpCatalog::clear()
{
    d->clear();
}

bool LumpCatalog::setPackages(const StringList &packageIds)
{
    if (packageIds != d->packageIds)
    {
        d->packageIds = packageIds;
        d->updateBundles();
        return true;
    }
    return false;
}

StringList LumpCatalog::packages() const
{
    return d->packageIds;
}

void LumpCatalog::setBundles(const List<const DataBundle *> &bundles)
{
    d->packageIds.clear();
    d->bundles = bundles;
}

LumpCatalog::LumpPos LumpCatalog::find(const String &lumpName) const
{
    return d->findLump(lumpName);
}

List<LumpCatalog::LumpPos> LumpCatalog::findAll(const String &lumpName) const
{
    return d->findAllLumps(lumpName);
}

List<LumpCatalog::LumpRange> LumpCatalog::flatRanges() const
{
    return d->flatRanges();
}

Block LumpCatalog::read(const String &lumpName) const
{
    return read(d->findLump(lumpName));
}
    
Block LumpCatalog::read(const LumpPos &lump) const
{
    Block data;
    if (lump.first)
    {
        DE_ASSERT(lump.first->lumpDirectory());
        const auto &entry = lump.first->lumpDirectory()->entry(lump.second);
        data.copyFrom(*lump.first, entry.offset, entry.size);
    }
    return data;
}

String LumpCatalog::lumpName(const LumpPos &lump) const
{
    if (lump.first)
    {
        DE_ASSERT(lump.first->lumpDirectory());
        return String::fromLatin1(lump.first->lumpDirectory()->entry(lump.second).name);
    }
    return {};
}

} // namespace res
