/** @file infobank.cpp  Abstract Bank read from Info definitions.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/infobank.h"
#include "de/app.h"
#include "de/folder.h"
#include "de/package.h"
#include "de/scripting/scriptedinfo.h"

namespace de {

DE_PIMPL(InfoBank)
, DE_OBSERVES(ScriptedInfo, NamedBlock)
{
    Record       names; ///< All parsed sources will be stored here.
    ScriptedInfo info{&names};
    Time         modTime;
    String       relativeToPath;
    String       containerPackage;

    static const String VAR_NOT_IN_BANK;

    Impl(Public *i) : Base(i)
    {
        info.audienceForNamedBlock() += this;
    }

    void parsedNamedBlock(const String &, Record &block) override
    {
        if (block.gets(ScriptedInfo::VAR_BLOCK_TYPE) != ScriptedInfo::BLOCK_GROUP)
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
                         std::function<bool (const String &, const Record &)> shouldRemove,
                         String identifierBase = {})
    {
        group.forSubrecords([&, this](const String &name, Record &sub)
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

                self().remove(fullIdentifier);
                delete group.remove(name);
            }
            return LoopContinue;
        });
    }
};

const String InfoBank::Impl::VAR_NOT_IN_BANK{"__notInBank__"};

InfoBank::InfoBank(const char *nameForLog, const Flags &flags, const String &hotStorageLocation)
    : Bank(nameForLog, flags, hotStorageLocation)
    , d(new Impl(this))
{}

void InfoBank::parse(const String &source)
{
    try
    {
        d->relativeToPath = "";
        d->modTime = Time();
        d->info.parse(source);
    }
    catch (const Error &er)
    {
        LOG_WARNING("Failed to read Info source:\n") << er.asText();
    }
}

void InfoBank::parse(const File &file)
{
    try
    {
        d->relativeToPath = file.path().fileNamePath();
        d->modTime = file.status().modifiedAt;
        d->info.parse(file);
    }
    catch (const Error &er)
    {
        LOG_WARNING("Failed to read Info file:\n") << er.asText();
    }
}

ScriptedInfo &InfoBank::info()
{
    return d->info;
}

const ScriptedInfo &InfoBank::info() const
{
    return d->info;
}

Record &InfoBank::objectNamespace()
{
    return d->names;
}

const Record &InfoBank::objectNamespace() const
{
    return d->names;
}

void InfoBank::addFromInfoBlocks(const String &blockType)
{
    for (const String &id : d->info.allBlocksOfType(blockType))
    {
        Record &rec = d->names[id];
        if (!rec.has(Impl::VAR_NOT_IN_BANK))
        {
            // Already added, from the looks of it.
            continue;
        }
        add(id, newSourceFromInfo(id));
        delete &rec[Impl::VAR_NOT_IN_BANK];
    }
}

void InfoBank::removeAllWithRootPath(const String &rootPath)
{
    LOG_AS(nameForLog());
    d->removeFromGroup(d->names, [&rootPath] (const String &, const Record &rec) {
        return ScriptedInfo::sourceLocation(rec).beginsWith(rootPath);
    });
}

void InfoBank::removeAllFromPackage(const String &packageId)
{
    LOG_AS(nameForLog());
    d->removeFromGroup(d->names, [&packageId](const String &, const Record &rec) {
        const auto  loc  = ScriptedInfo::sourcePathAndLine(rec);
        const auto &file = App::rootFolder().locate<const File>(loc.first);
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

String InfoBank::absolutePathInContext(const Record &context, const String &relativePath) const
{
    if (Path(relativePath).isAbsolute())
    {
        // Already absolute.
        return relativePath;
    }

    const String path = ScriptedInfo::absolutePathInContext(context, relativePath);
    if (path == relativePath)
    {
        return bankRootPath() / relativePath;
    }
    return path;
}

} // namespace de
