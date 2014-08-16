/** @file infobank.cpp  Abstract Bank read from Info definitions.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/InfoBank"
#include "de/ScriptedInfo"
#include "de/File"
#include "de/App"

namespace de {

static String const VAR_NOT_IN_BANK("__notInBank__");

DENG2_PIMPL_NOREF(InfoBank)
, DENG2_OBSERVES(ScriptedInfo, NamedBlock)
{
    Record names; ///< All parsed sources will be stored here.
    ScriptedInfo info { &names };
    Time modTime;
    String relativeToPath;

    Instance()
    {
        info.audienceForNamedBlock() += this;
    }

    void parsedNamedBlock(String const &, Record &block)
    {
        if(block.gets("__type__") != "group")
        {
            block.addBoolean(VAR_NOT_IN_BANK, true);
        }
    }
};

InfoBank::InfoBank(char const *nameForLog, Bank::Flags const &flags, String const &hotStorageLocation)
    : Bank(nameForLog, flags, hotStorageLocation), d(new Instance)
{}

void InfoBank::parse(String const &source)
{
    try
    {
        d->relativeToPath = "";
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
    try
    {
        d->relativeToPath = file.path().fileNamePath();
        d->modTime = file.status().modifiedAt;
        d->info.parse(file);
    }
    catch(Error const &er)
    {
        LOG_WARNING("Failed to read Info file:\n") << er.asText();
    }
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
    return d->names;
}

Record const &InfoBank::names() const
{
    return d->names;
}

void InfoBank::addFromInfoBlocks(String const &blockType)
{
    foreach(String id, d->info.allBlocksOfType(blockType))
    {
        Record &rec = names()[id];
        if(!rec.has(VAR_NOT_IN_BANK))
        {
            // Already added, from the looks of it.
            continue;
        }

        add(id, newSourceFromInfo(id));

        delete &rec[VAR_NOT_IN_BANK];
    }
}

Time InfoBank::sourceModifiedAt() const
{
    return d->modTime;
}

String InfoBank::bankRootPath() const
{
    return d->relativeToPath;
}

String InfoBank::relativeToPath(Record const &context) const
{
    String const root = ScriptedInfo::absolutePathInContext(context, "");
    if(root.isEmpty()) return bankRootPath();
    return root;
}

Variable const &InfoBank::operator [] (String const &name) const
{
    return d->info[name];
}

} // namespace de
