/** @file bundles.cpp  Data bundle indexing.
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

#include "doomsday/resource/bundles.h"
#include "doomsday/filesys/datafile.h"
#include "doomsday/filesys/datafolder.h"

#include <de/App>
#include <de/Loop>

#include <QList>

using namespace de;

namespace res {

DENG2_PIMPL(Bundles)
, DENG2_OBSERVES(FileIndex, Addition)
{
    de::Info identityRegistry;
    QList<DataBundle const *> bundlesToIdentify;
    LoopCallback mainCall;
    QHash<DataBundle::Format, BlockElements> formatEntries;

    Instance(Public *i) : Base(i)
    {
        // Observe new data files.
        App::fileSystem().indexFor(DENG2_TYPE_NAME(DataFile))  .audienceForAddition() += this;
        App::fileSystem().indexFor(DENG2_TYPE_NAME(DataFolder)).audienceForAddition() += this;
    }

    void fileAdded(File const &dataFile, FileIndex const &)
    {
        DENG2_ASSERT(dataFile.is<DataBundle>());
        bundlesToIdentify << dataFile.maybeAs<DataBundle>();
        if(mainCall.isEmpty())
        {
            mainCall.enqueue([this] () { identifyAddedDataBundles(); });
        }
    }

    void identifyAddedDataBundles()
    {
        DENG2_ASSERT(App::rootFolder().has("/sys/bundles"));

        LOG_RES_MSG("Identifying %i data bundles") << bundlesToIdentify.size();

        for(DataBundle const *bundle : bundlesToIdentify)
        {
            DENG2_ASSERT(bundle);

            File const *f = dynamic_cast<File const *>(bundle);
            qDebug() << bundle->description() << f->path();

            bundle->identifyPackages();
        }

        bundlesToIdentify.clear();
    }

    void parseRegistry()
    {
        using Info = de::Info;

        if(!identityRegistry.isEmpty()) return;

        String const defPath = "/packs/net.dengine.base/databundles.dei";

        formatEntries.clear();
        identityRegistry.parse(App::rootFolder().locate<File const>(defPath));

        for(auto const *elem : identityRegistry.root().contentsInOrder())
        {
            if(!elem->isBlock()) continue;

            Info::BlockElement const &block = elem->as<Info::BlockElement>();
            if(block.blockType() != QStringLiteral("package"))
            {
                // Not sure what this is...
                continue;
            }

            String format = block.keyValue(QStringLiteral("format")).text.toLower();

            DataBundle::Format bundleFormat =
                    (format == "iwad"? DataBundle::Iwad :
                     format == "pwad"? DataBundle::Pwad :
                     format == "pk3"?  DataBundle::Pk3 :
                     format == "lmp"?  DataBundle::Lump :
                     format == "deh"?  DataBundle::Dehacked :
                     format == "ded"?  DataBundle::Ded :
                                       DataBundle::Unknown);

            if(bundleFormat == DataBundle::Unknown)
            {
                throw InvalidError("Bundles::parseRegistry",
                                   defPath + ": invalid format for \"" + block.name() + "\"");
            }

            formatEntries[bundleFormat].append(&block);
        }
    }
};

Bundles::Bundles()
    : d(new Instance(this))
{}

de::Info const &Bundles::identityRegistry() const
{
    d->parseRegistry();
    return d->identityRegistry;
}

res::Bundles::BlockElements Bundles::formatEntries(DataBundle::Format format) const
{
    d->parseRegistry();
    return d->formatEntries[format];
}

void Bundles::identify()
{
    d->identifyAddedDataBundles();
}

} // namespace res
