/** @file idgamespackageinfofile.cpp
 *
 * @authors Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "doomsday/filesys/idgamespackageinfofile.h"
#include "doomsday/res/bundles.h"
#include "doomsday/doomsdayapp.h"

#include <de/arrayvalue.h>
#include <de/filesystem.h>
#include <de/folder.h>
#include <de/package.h>
#include <de/textvalue.h>
#include <de/regexp.h>

using namespace de;

DE_PIMPL(IdgamesPackageInfoFile)
, DE_OBSERVES(Asset, StateChange)
{
    Asset packageAsset;
    AssetGroup assets;
    SafePtr<RemoteFile const> dataFile;
    SafePtr<RemoteFile const> descriptionFile;
    Block serializedContent;

    Impl(Public *i) : Base(i)
    {
        assets.audienceForStateChange() += this;
    }

    String cacheFolderPath() const
    {
        const String hex = dataFile->metaId().asHexadecimalText();
        return RemoteFile::CACHE_PATH / hex.right(CharPos(1)) / hex;
    }

    void assetStateChanged(Asset &)
    {
        if (!assets.isEmpty() && assets.isReady())
        {
            // Looks like we can process the file contents.
            debug("[IdgamesPackageInfoFile] Time to unzip and analyze!");

            DE_ASSERT(dataFile->isReady());
            DE_ASSERT(descriptionFile->isReady());

            // We need to extract data files (WAD, DEH) so that they can be loaded.
            /// @todo No need for this after FS2 used for loading everything.

            auto &fs = FileSystem::get();

            Folder &packFolder = *self().parent();
            DotPath const packageId { packFolder.name() };

            /*Folder &cacheFolder = */fs.makeFolder(cacheFolderPath());

            FS::copySerialized(descriptionFile->path(),
                               cacheFolderPath()/descriptionFile->name());

            StringList dataFiles;
            Record meta;

            // A ZIP from idgames can contain any number of data files to load. Even
            // though some ZIPs only have one WAD file, the following doesn't make a
            // special case for those. (Single-file packages could just as well be
            // recognized directly as an idgames package.)

            // If the data file is a ZIP archive, let's see what it contains. We want to
            // have the data file(s) and the description text in the same folder so
            // DataBundle will analyze it fully.

//            qDebug() << dataFile->parent()->path() << "\n" << dataFile->parent()->contentsAsText();
//            qDebug() << dataFile->target().size() << dataFile->target().status().modifiedAt.asText();

            //qDebug() << "remote file timestamp:" << DataBundle::versionFromTimestamp(dataFile->target().status().modifiedAt);
            meta.set("title",   dataFile->name().fileNameWithoutExtension().upper() + " (idgames)");
            meta.set("version", DataBundle::versionFromTimestamp(dataFile->target().status().modifiedAt));

            if (Folder *zip = const_cast<Folder *>(maybeAs<Folder>(dataFile->target())))
            {
                zip->populate();
                zip->forContents([this, &dataFiles] (String name, File &file)
                {
                    const String ext = name.fileNameExtension().lower();
                    if (ext == ".wad" || ext == ".deh" || ext == ".lmp" || ext == ".pk3")
                    {
                        File &copied = FS::copySerialized
                                (file.path(), cacheFolderPath()/file.name());
                        dataFiles << copied.path();
                    }
                    return LoopContinue;
                });
                FS::waitForIdle();

                StringList components;
                for (String path : dataFiles)
                {
                    if (const DataBundle *bundle = FS::tryLocate<DataBundle const>(path))
                    {
                        //components << bundle->versionedPackageId();
                        components << bundle->asFile().path();

                        const Record &compMeta = bundle->packageMetadata();
                        if (compMeta.has("notes"))
                        {
                            meta.set("notes", compMeta.gets("notes"));
                            if (compMeta.has("title"))
                            {
                                meta.set("title", compMeta.gets("title"));
                            }
//                            if (compMeta.has("version"))
//                            {
//                                meta.set("version", compMeta.gets("version"));
//                            }
                            if (compMeta.has("license"))
                            {
                                meta.set("license", compMeta.gets("license"));
                            }
                            if (compMeta.has("author"))
                            {
                                meta.set("author", compMeta.gets("author"));
                            }
                            if (compMeta.has("tags"))
                            {
                                meta.appendMultipleUniqueWords("tags", compMeta.gets("tags"));
                            }
                        }
                    }
                }
                std::unique_ptr<ArrayValue> comps { new ArrayValue };
                for (const String &comp : components)
                {
                    comps->add(new TextValue(comp));
                }
                meta.addArray("dataFiles", comps.release());

                meta.set("tags", meta.gets("tags", "")
                         .removed(RegExp("\\b(hidden|cached)\\b"))
                         .normalizeWhitespace());

                // Version should match the idgames index version (or overridden from
                // metadata with an actual version).
                debug("idgames package will contain: %s", String::join(components, " ").c_str());
                debug(meta.asText().c_str());

                if (packageId.segment(1) == "levels")
                {
                    // Tag with the right game.
                    meta.set("tags", meta.gets("tags", "")
                             .removed(RegExp(DataBundle::anyGameTagPattern()))
                             .normalizeWhitespace());
                    meta.appendUniqueWord("tags", packageId.segment(2).toLowercaseString());
                }

                // Apply metadata to the folder representing the package.
                auto &pkgMeta = Package::initializeMetadata(packFolder, packageId);
                pkgMeta.copyMembersFrom(meta);

                // Prepare the required package info in case this is installed locally.
                {
                    Record info { pkgMeta, Record::IgnoreDoubleUnderscoreMembers };
                    delete info.remove("ID");
                    delete info.remove("path");
                    const String cachePath = cacheFolderPath();
                    ArrayValue *pkgData = new ArrayValue;
                    for (String path : info.getStringList("dataFiles"))
                    {
                        if (path.beginsWith(cachePath))
                        {
                            path.remove(BytePos(0), cachePath.size() + 1);
                        }
                        pkgData->add(new TextValue(path));
                    }
                    info.set("dataFiles", pkgData);
                    serializedContent = info.asInfo().toUtf8();
                }
            }

            // Everythis is complete.
            DE_NOTIFY_PUBLIC_VAR(Download, i)
            {
                i->downloadProgress(self(), 0);
            }
            packageAsset.setState(Asset::Ready);
        }
    }
};

IdgamesPackageInfoFile::IdgamesPackageInfoFile(const String &name)
    : File(name)
    , d(new Impl(this))
{}

void IdgamesPackageInfoFile::setSourceFiles(const RemoteFile &dataFile,
                                            const RemoteFile &descriptionFile)
{
    d->dataFile       .reset(&dataFile);
    d->descriptionFile.reset(&descriptionFile);

    d->assets += dataFile.asset();
    d->assets += descriptionFile.asset();
}

Asset &IdgamesPackageInfoFile::asset()
{
    return d->packageAsset;
}

const Asset &IdgamesPackageInfoFile::asset() const
{
    return d->packageAsset;
}

dsize IdgamesPackageInfoFile::downloadSize() const
{
    // No additional download for the info file is required.
    return 0;
}

void IdgamesPackageInfoFile::download()
{}

void IdgamesPackageInfoFile::cancelDownload()
{}

const IIStream &IdgamesPackageInfoFile::operator>>(IByteArray &bytes) const
{
    bytes.set(0, d->serializedContent.data(), d->serializedContent.size());
    return *this;
}
