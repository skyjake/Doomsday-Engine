/** @file infobank.cpp  Abstract Bank read from Info definitions.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/InfoBank"
#include "de/ScriptedInfo"
#include "de/File"

namespace de {

DENG2_PIMPL(InfoBank)
{
    ScriptedInfo info;
    Time modTime;
};

InfoBank::InfoBank(Bank::Flags const &flags, String const &hotStorageLocation)
    : Bank(flags, hotStorageLocation)
{}

void InfoBank::parse(String const &source)
{
    try
    {
        d->modTime = Time();
        d->info.parse(source);
    }
    catch(Error const &er)
    {
        LOG_WARNING("Failed to read Info source:\n") << er.asText();
    }
}

void InfoBank::parse(File const &file)
{
    parse(String::fromUtf8(Block(file)));
    d->modTime = file.status().modifiedAt;
}

ScriptedInfo &InfoBank::info()
{
    return d->info;
}

ScriptedInfo const &InfoBank::info() const
{
    return d->info;
}

Record &InfoBank::names()
{
    return d->info.names();
}

Record const &InfoBank::names() const
{
    return d->info.names();
}

void InfoBank::addFromInfo(String const &blockType)
{
    foreach(String id, d->info.allBlocksOfType(blockType))
    {
        add(id, newSourceFromInfo(id));
    }
}

Time InfoBank::sourceModifiedAt() const
{
    return d->modTime;
}

Variable const &InfoBank::operator [] (String const &name) const
{
    return d->info[name];
}

} // namespace de
