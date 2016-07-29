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
#include "de/App"
#include "de/Folder"
#include "de/Package"
#include "de/ScriptedInfo"

namespace de {

static String const VAR_NOT_IN_BANK("__notInBank__");

DENG2_PIMPL(InfoBank)
, DENG2_OBSERVES(ScriptedInfo, NamedBlock)
{
    Record names; ///< All parsed sources will be stored here.
    ScriptedInfo info { &names };
    Time modTime;
    String relativeToPath;

    Impl(Public *i) : Base(i)
    {
        info.audienceForNamedBlock() += this;
    }

    void parsedNamedBlock(String const &, Record &block)
    {
        if (block.gets("__type__") != ScriptedInfo::BLOCK_GROUP)
        {
            block.addBoolean(VAR_NOT_IN_BANK, true);
        }
    }

    /**
     * Checks the contents of @a group and removes all blocks that return
     * @c true from @a shouldRemove. The removal is done both from the Bank and
     * the script namespace. Subgroups are checked recursively.
     */
    void removeFromGroup(Record &group,
                         std::function<bool (String const &, Record const &)> shouldRemove,
                         String identifierBase = "")
    {
        group.forSubrecords([this, &group, &shouldRemove, &identifierBase]
                            (String const &name, Record &sub)
        {
            String fullIdentifier = identifierBase.concatenateMember(name);
            if (ScriptedInfo::blockType(sub) == ScriptedInfo::BLOCK_GROUP)
            {
                removeFromGroup(sub, shouldRemove, fullIdentifier);
            }
            else if (shouldRemove(name, sub))
            {
                LOG_RES_VERBOSE("Removing '%s' that was read from \"%s\"")
                        << fullIdentifier
                        << ScriptedInfo::sourceLocation(sub);

                self.remove(fullIdentifier);
                delete group.remove(name);
            }
            return LoopContinue;
        });
    }
};

InfoBank::InfoBank(char const *nameForLog, Bank::Flags const &flags, String const &hotStorageLocation)
    : Bank(nameForLog, flags, hotStorageLocation)
    , d(new Impl(this))
{}

void InfoBank::parse(String const &source)
{
    try
    {
        d->relativeToPath = "";
        d->modTime = Time();
        d->info.parse(source);
    }
    catch (Error const &er)
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
    catch (Error const &er)
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

Record &InfoBank::objectNamespace()
{
    return d->names;
}

Record const &InfoBank::objectNamespace() const
{
    return d->names;
}

void InfoBank::addFromInfoBlocks(String const &blockType)
{
    foreach (String id, d->info.allBlocksOfType(blockType))
    {
        Record &rec = d->names[id];
        if (!rec.has(VAR_NOT_IN_BANK))
        {
            // Already added, from the looks of it.
            continue;
        }

        add(id, newSourceFromInfo(id));

        delete &rec[VAR_NOT_IN_BANK];
    }
}

void InfoBank::removeAllWithRootPath(String const &rootPath)
{
    LOG_AS(nameForLog());
    d->removeFromGroup(d->names, [&rootPath] (String const &, Record const &rec) {
        return ScriptedInfo::sourceLocation(rec).startsWith(rootPath);
    });
}

void InfoBank::removeAllFromPackage(String const &packageId)
{
    LOG_AS(nameForLog());
    d->removeFromGroup(d->names, [&packageId] (String const &, Record const &rec) {
        auto const loc = ScriptedInfo::sourcePathAndLine(rec);
        File const &file = App::rootFolder().locate<File const>(loc.first);
        return Package::identifierForContainerOfFile(file) == packageId;
    });
}

Time InfoBank::sourceModifiedAt() const
{
    return d->modTime;
}

String InfoBank::bankRootPath() const
{
    return d->relativeToPath;
}

String InfoBank::absolutePathInContext(Record const &context, String const &relativePath) const
{
    if (Path(relativePath).isAbsolute())
    {
        // Already absolute.
        return relativePath;
    }

    String const path = ScriptedInfo::absolutePathInContext(context, relativePath);
    if (path == relativePath)
    {
        return bankRootPath() / relativePath;
    }
    return path;
}

} // namespace de
