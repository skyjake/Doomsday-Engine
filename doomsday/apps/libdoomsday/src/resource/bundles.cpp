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

Bundles::BlockElements Bundles::formatEntries(DataBundle::Format format) const
{
    d->parseRegistry();
    return d->formatEntries[format];
}

void Bundles::identify()
{
    d->identifyAddedDataBundles();

    //qDebug() << App::rootFolder().locate<Folder const>("/sys/bundles").contentsAsText();
}

Bundles::MatchResult Bundles::match(DataBundle const &bundle) const
{
    using Info = de::Info;

    MatchResult match;
    File const &source = bundle.asFile();

    // Find the best match from the registry.
    for(auto const *def : formatEntries(bundle.format()))
    {
        int score = 0;

        // Match the file name.
        if(auto const *fileName = def->find(QStringLiteral("fileName")))
        {
            if(fileName->isKey() &&
               fileName->as<Info::KeyElement>().value()
                    .text.compareWithoutCase(source.name()) == 0)
            {
                ++score;
            }
            else if(fileName->isList())
            {
                // Any of the provided alternatives will be accepted.
                for(auto const &cand : fileName->as<Info::ListElement>().values())
                {
                    if(!cand.text.compareWithoutCase(source.name()))
                    {
                        ++score;
                        break;
                    }
                }
            }
        }

        // Match the file size.
        String fileSize = def->keyValue(QStringLiteral("fileSize"));
        if(!fileSize.isEmpty() && fileSize.toUInt() == source.size())
        {
            ++score;
        }

        bool crcMismatch = false;

        // Additional criteria for recognizing WADs.
        if(bundle.format() == DataBundle::Iwad ||
           bundle.format() == DataBundle::Pwad)
        {
            String lumpDirCRC32 = def->keyValue(QStringLiteral("lumpDirCRC32"));
            if(!lumpDirCRC32.isEmpty())
            {
                if(lumpDirCRC32.toUInt(nullptr, 16) == bundle.lumpDirectory()->crc32())
                {
                    // Low probability of a false negative => more significant.
                    score += 2;
                }
                else
                {
                    crcMismatch = true;
                }
            }

            if(auto const *lumps = def->find(QStringLiteral("lumps"))->maybeAs<Info::ListElement>())
            {
                ++score; // will be subtracted if not matched

                for(auto const &val : lumps->values())
                {
                    QRegExp const sizeCondition("(.*)==([0-9]+)");
                    Block lumpName;
                    int requiredSize = 0;

                    if(sizeCondition.exactMatch(val))
                    {
                        lumpName     = sizeCondition.cap(1).toUtf8();
                        requiredSize = sizeCondition.cap(2).toInt();
                    }
                    else
                    {
                        lumpName     = val.text.toUtf8();
                        requiredSize = -1;
                    }

                    if(!bundle.lumpDirectory()->has(lumpName))
                    {
                        --score;
                        break;
                    }

                    if(requiredSize >= 0 &&
                       bundle.lumpDirectory()->lumpSize(lumpName) != duint32(requiredSize))
                    {
                        --score;
                        break;
                    }
                }
            }
        }

        if(score > 0 && score >= match.bestScore)
        {
            match.bestMatch = def;
            match.bestScore = score;

            auto const idVer = Package::split(def->name());

            match.packageId = idVer.first;
            // If the specified CRC32 doesn't match, we can't be certain of
            // which version this actually is.
            match.packageVersion = (!crcMismatch? idVer.second : Version(""));
        }
    }

    qDebug() << "[res::Bundles] Matched:" << match.packageId
             << match.packageVersion.asText()
             << bundle.description()
             << "score:" << match.bestScore;

    return match;
}

} // namespace res
