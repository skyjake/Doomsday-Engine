/** @file idtech1flatlib.cpp
 *
 * @authors Copyright (c) 2018 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "doomsday/res/idtech1flatlib.h"
#include "doomsday/res/idtech1util.h"

namespace res {

DE_PIMPL(IdTech1FlatLib)
{
    const LumpCatalog &catalog;
    KeyMap<String, LumpCatalog::LumpPos, String::InsensitiveLessThan> flats;
    Block palette;

    Impl(Public *i, const LumpCatalog &catalog) : Base(i), catalog(catalog)
    {
        init();
    }

    void init()
    {
        palette = catalog.read("PLAYPAL");

        const auto flatRanges = catalog.flatRanges();
        for (const auto &range : flatRanges)
        {
            const auto *direc = range.first->lumpDirectory();

            // All lumps inside the range(s) are considered flats.
            for (LumpDirectory::Pos pos = range.second.start; pos < range.second.end; ++pos)
            {
                const String name = String::fromLatin1(direc->entry(pos).name);
                if (!flats.contains(name))
                {
                    flats.insert(name, {range.first, pos});
                }
            }
        }
    }
};

IdTech1FlatLib::IdTech1FlatLib(const LumpCatalog &catalog)
    : d(new Impl(this, catalog))
{}

IdTech1Image IdTech1FlatLib::flatImage(const String &name) const
{
    static const Vec2ui flatSize{64, 64};

    const auto *_d = d.get();

    auto found = _d->flats.find(name);
    if (found != _d->flats.end())
    {
        return IdTech1Image(flatSize, _d->catalog.read(found->second), _d->palette);
    }
    return {};
}

} // namespace res
