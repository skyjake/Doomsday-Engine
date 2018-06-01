/** @file bundles.cpp  Data bundle indexing.
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

#include "doomsday/resource/bundles.h"
#include "doomsday/filesys/datafile.h"
#include "doomsday/filesys/datafolder.h"

#include <de/App>
#include <de/Config>
#include <de/DictionaryValue>
#include <de/PackageLoader>
#include <de/LinkFile>
#include <de/Loop>
#include <de/TaskPool>

#include <QList>
#include <QSet>

using namespace de;

namespace res {

static int const MATCH_MAXIMUM_SCORE = 4; // in case 5 specified, allow 1 to not match for flexibility

static String const VAR_REQUIRED_SCORE = "requiredScore";

DE_PIMPL(Bundles)
, DE_OBSERVES(FileIndex, Addition)
, DE_OBSERVES(FileIndex, Removal)
, public Lockable
{
    String defPath;
    de::Info identityRegistry;
    QSet<DataBundle const *> bundlesToIdentify; // lock for access
    LoopCallback mainCall;
    QHash<DataBundle::Format, BlockElements> formatEntries;
    TaskPool tasks;

    Impl(Public * i, String const &bundleDefPath)
        : Base(i)
        , defPath(bundleDefPath)
    {
        // Observe new data files.
        App::fileSystem().indexFor(DE_TYPE_NAME(DataFile))  .audienceForAddition() += this;
        App::fileSystem().indexFor(DE_TYPE_NAME(DataFile))  .audienceForRemoval()  += this;
        App::fileSystem().indexFor(DE_TYPE_NAME(DataFolder)).audienceForAddition() += this;
        App::fileSystem().indexFor(DE_TYPE_NAME(DataFolder)).audienceForRemoval()  += this;
    }

    ~Impl()
    {
        // Ongoing identification tasks should first finish.
        {
            DE_GUARD(this);
            bundlesToIdentify.clear();
        }
        tasks.waitForDone();
    }

    void fileAdded(File const &dataFile, FileIndex const &)
    {
        DE_ASSERT(is<DataBundle>(dataFile));
        {
            DE_GUARD(this);
            bundlesToIdentify.insert(maybeAs<DataBundle>(dataFile));
        }
        // Use a deferred call to avoid spamming.
        if (!mainCall)
        {
            mainCall.enqueue([this] () { self().identify(); });
        }
    }

    void fileRemoved(File const &dataFile, FileIndex const &)
    {
        DE_ASSERT(is<DataBundle>(dataFile));

        DE_GUARD(this);
        bundlesToIdentify.remove(maybeAs<DataBundle>(dataFile));
    }

    DataBundle const *nextToIdentify()
    {
        DE_GUARD(this);
        if (bundlesToIdentify.isEmpty())
        {
            return nullptr;
        }
        auto const *bundle = *bundlesToIdentify.begin();
        bundlesToIdentify.remove(bundle);
        return bundle;
    }

    bool identifyAddedDataBundles()
    {
        Folder::waitForPopulation();

        DE_ASSERT(App::rootFolder().has("/sys/bundles"));

        bool wasIdentified = false;
        int  count         = 0;
        Time startedAt;

        while (auto const *bundle = nextToIdentify())
        {
            ++count;
            if (bundle->identifyPackages())
            {
                wasIdentified = true;
            }
        }
        if (count)
        {
            LOG_RES_MSG("Identified %i data bundles in %.1f seconds") << count << startedAt.since();
        }
        return wasIdentified;
    }

    void parseRegistry()
    {
        DENG2_GUARD(this);

        if (!identityRegistry.isEmpty()) return;

        formatEntries.clear();
        identityRegistry.parse(App::rootFolder().locate<File const>(defPath));

        for (auto *elem : identityRegistry.root().contentsInOrder())
        {
            using Info = de::Info;

            if (!elem->isBlock())
            {
                // Looking for "package" blocks.
                continue;
            }

            Info::BlockElement &block = elem->as<Info::BlockElement>();
            if (block.blockType() != QStringLiteral("package"))
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

            if (bundleFormat == DataBundle::Unknown)
            {
                throw InvalidError("Bundles::parseRegistry",
                                   defPath + ": invalid format for \"" + block.name() + "\"");
            }

            // How many rules required?
            if (!block.contains(VAR_REQUIRED_SCORE))
            {
                int const ruleCount = block.size() - 1; // not counting "info"
                block.add(new Info::KeyElement(VAR_REQUIRED_SCORE,
                        String::format("%i", de::min(MATCH_MAXIMUM_SCORE, ruleCount))));
            }

            formatEntries[bundleFormat].append(&block);
        }
    }

    DE_PIMPL_AUDIENCE(Identify)
};

DE_AUDIENCE_METHOD(Bundles, Identify)

Bundles::Bundles(const String &bundleDefPath)
    : d(new Impl(this, bundleDefPath))
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
    FS::get().changeBusyLevel(+1);
    d->tasks.start([this] ()
    {
        d->identifyAddedDataBundles();
        if (isEverythingIdentified())
        {
            DE_FOR_AUDIENCE2(Identify, i)
            {
                i->dataBundlesIdentified();
            }
        }
        FS::get().changeBusyLevel(-1);
    });
}

bool Bundles::isEverythingIdentified() const
{
    DE_GUARD(d);
    return d->bundlesToIdentify.isEmpty();
}

/*void Bundles::waitForEverythingIdentified()
{
    identify();
    d->tasks.waitForDone();
}*/

Bundles::MatchResult Bundles::match(DataBundle const &bundle) const
{
    using Info = de::Info;

    LOG_AS("res::Bundles");

    MatchResult match;
    File const &source = bundle.asFile();

    // Find the best match from the registry.
    for (auto const *def : formatEntries(bundle.format()))
    {
        int score = 0;

        // Match the file name.
        if (auto const *fileName = def->find(QStringLiteral("fileName")))
        {
            if (fileName->isKey() &&
                fileName->as<Info::KeyElement>().value()
                    .text.compareWithoutCase(source.name()) == 0)
            {
                ++score;
            }
            else if (fileName->isList())
            {
                // Any of the provided alternatives will be accepted.
                for (auto const &cand : fileName->as<Info::ListElement>().values())
                {
                    if (!cand.text.compareWithoutCase(source.name()))
                    {
                        ++score;
                        break;
                    }
                }
            }
        }

        // Match the file type.
        String fileType = def->keyValue(QStringLiteral("fileType"));
        if (fileType.isEmpty()) fileType = "file"; // prefer files by default
        if ((!fileType.compareWithoutCase(QStringLiteral("file"))   && source.status().type() == File::Type::File) ||
            (!fileType.compareWithoutCase(QStringLiteral("folder")) && source.status().type() == File::Type::Folder))
        {
            ++score;
        }

        // Match the file size.
        String fileSize = def->keyValue(QStringLiteral("fileSize"));
        if (!fileSize.isEmpty() && fileSize.toUInt() == source.size())
        {
            ++score;
        }

        bool crcMismatch = false;

        // Additional criteria for recognizing WADs.
        if (bundle.format() == DataBundle::Iwad ||
            bundle.format() == DataBundle::Pwad)
        {
            String lumpDirCRC32 = def->keyValue(QStringLiteral("lumpDirCRC32"));
            if (!lumpDirCRC32.isEmpty())
            {
                if (lumpDirCRC32.toUInt(nullptr, 16) == bundle.lumpDirectory()->crc32())
                {
                    // Low probability of a false negative => more significant.
                    score += 2;
                }
                else
                {
                    crcMismatch = true;
                }
            }

            if (auto const *lumps = maybeAs<Info::ListElement>(def->find(QStringLiteral("lumps"))))
            {
                ++score; // will be subtracted if not matched

                for (auto const &val : lumps->values())
                {
                    QRegExp const sizeCondition("(.*)==([0-9]+)");
                    Block lumpName;
                    int requiredSize = 0;

                    if (sizeCondition.exactMatch(val))
                    {
                        lumpName     = sizeCondition.cap(1).toUtf8();
                        requiredSize = sizeCondition.cap(2).toInt();
                    }
                    else
                    {
                        lumpName     = val.text.toUtf8();
                        requiredSize = -1;
                    }

                    if (!bundle.lumpDirectory()->has(lumpName))
                    {
                        --score;
                        break;
                    }

                    if (requiredSize >= 0 &&
                        bundle.lumpDirectory()->lumpSize(lumpName) != duint32(requiredSize))
                    {
                        --score;
                        break;
                    }
                }
            }
        }

        if (score < def->keyValue(VAR_REQUIRED_SCORE).text.toInt())
        {
            score = 0;
        }

        if (score > 0 && score >= match.bestScore)
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

    if (!match.bestScore)
    {
        // No go.
        return MatchResult();
    }

    LOG_RES_VERBOSE("Matched: %s %s %s score: %i")
            << match.packageId
            << match.packageVersion.fullNumber()
            << bundle.description()
            << match.bestScore;

    return match;
}

} // namespace res
