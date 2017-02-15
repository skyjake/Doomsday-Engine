/** @file metadatabank.cpp  Cache for file metadata.
 *
 * @authors Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de/MetadataBank"
#include "de/App"
#include "de/FileSystem"
#include "de/Folder"

namespace de {

DENG2_PIMPL(MetadataBank), public Lockable
{
    struct Source : public ISource
    {
        Block metaId;

        Source(Block const &id) : metaId(id) {}
    };

    struct Data : public IData
    {
        Block metadata;
        bool isChanged = false;

        bool shouldBeSerialized() const override {
            return isChanged;
        }
        ISerializable *asSerializable() override {
            return &metadata;
        }
        virtual duint sizeInMemory() const override {
            return duint(metadata.size());
        }
    };

    Impl(Public *i) : Base(i) {}

    static DotPath pathFromId(String const &category, Block const &id)
    {
        DENG2_ASSERT(!id.isEmpty());
        String const hex = id.asHexadecimalText();
        return String("%1.%2.%3").arg(category).arg(hex.last()).arg(hex);
    }
};

MetadataBank::MetadataBank()
    : Bank("MetadataBank", SingleThread | EnableHotStorage, "/home/cache/metadata")
    , d(new Impl(this))
{}

MetadataBank &MetadataBank::get() // static
{
    return App::metadataBank();
}

Block MetadataBank::check(String const &category, Block const &id)
{
    DENG2_GUARD(d);

    DotPath const path = Impl::pathFromId(category, id);
    if (!has(path))
    {
        Bank::add(path, new Impl::Source(id));
    }
    return data(path).as<Impl::Data>().metadata;
}

void MetadataBank::setMetadata(String const &category, Block const &id, Block const &metadata)
{
    DENG2_GUARD(d);

    DotPath const path = Impl::pathFromId(category, id);
    if (!has(path))
    {
        Bank::add(path, new Impl::Source(id));
    }
    auto &entry = data(path).as<Impl::Data>();
    entry.metadata = metadata;
    entry.isChanged = true;
}

Block MetadataBank::metadata(String const &category, Block const &id) const
{
    DENG2_GUARD(d);

    return data(Impl::pathFromId(category, id)).as<Impl::Data>().metadata;
}

Bank::IData *MetadataBank::loadFromSource(ISource &)
{
    // The cached metadata can only be deserialized from hot storage or replaced.
    // Loading from source is not possible.
    return newData();
}

Bank::IData *MetadataBank::newData()
{
    return new Impl::Data;
}

} // namespace de
