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
#include "doomsday/res/Bundles"
#include "doomsday/DoomsdayApp"

#include <de/ArrayValue>
#include <de/FileSystem>
#include <de/Folder>
#include <de/Package>
#include <de/TextValue>

#include <QRegularExpression>

using namespace de;

DENG2_PIMPL(IdgamesPackageInfoFile)
, DENG2_OBSERVES(Asset, StateChange)
{
    Asset packageAsset;
    AssetGroup assets;
    SafePtr<RemoteFile const> dataFile;
    SafePtr<RemoteFile const> descriptionFile;

    Impl(Public *i) : Base(i)
    {
        assets.audienceForStateChange() += this;
    }

    String cacheFolderPath() const
    {
        String const hex = dataFile->metaId().asHexadecimalText();
        return RemoteFile::CACHE_PATH / hex.right(1) / hex;
    }

    void assetStateChanged(Asset &)
    {
        if (!assets.isEmpty() && assets.isReady())
        {
            // Looks like we can process the file contents.
            qDebug() << "[IdgamesPackageInfoFile] Time to unzip and analyze!";

            DENG2_ASSERT(dataFile->isReady());
            DENG2_ASSERT(descriptionFile->isReady());

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

//            qDebug() << dataFile->parent()->path() << "\n" << dataFile->parent()->contentsAsText().toUtf8().constData();
//            qDebug() << dataFile->target().size() << dataFile->target().status().modifiedAt.asText();

            //qDebug() << "remote file timestamp:" << DataBundle::versionFromTimestamp(dataFile->target().status().modifiedAt);
            meta.set("title",   dataFile->name().fileNameWithoutExtension().toUpper() + " (idgames)");
            meta.set("version", DataBundle::versionFromTimestamp(dataFile->target().status().modifiedAt));

            if (Folder *zip = const_cast<Folder *>(maybeAs<Folder>(dataFile->target())))
            {
                zip->populate();
                zip->forContents([this, &dataFiles] (String name, File &file)
                {
                    String const ext = name.fileNameExtension().toLower();
                    if (ext == ".wad" || ext == ".deh" || ext == ".lmp" || ext == ".pk3")
                    {
                        File &copied = FS::copySerialized
                                (file.path(), cacheFolderPath()/file.name());
                        dataFiles << copied.path();
                    }
                    return LoopContinue;
                });
                DoomsdayApp::bundles().waitForEverythingIdentified();

                StringList components;
                foreach (String path, dataFiles)
                {
                    if (DataBundle const *bundle = FS::tryLocate<DataBundle const>(path))
                    {
                        //components << bundle->versionedPackageId();
                        components << bundle->asFile().path();

                        Record const &compMeta = bundle->packageMetadata();
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
                foreach (String comp, components)
                {
                    comps->add(new TextValue(comp));
                }
                meta.addArray("dataFiles", comps.release());

                meta.set("tags", meta.gets("tags", "")
                         .removed(QRegularExpression("\\b(hidden|cached)\\b"))
                         .normalizeWhitespace());

                // Version should match the idgames index version (or overridden from
                // metadata with an actual version).
                qDebug() << "idgames package will contain:" << components;
                qDebug() << meta.asText().toUtf8().constData();

                if (packageId.segment(1) == "levels")
                {
                    // Tag with the right game.
                    meta.set("tags", meta.gets("tags", "")
                             .removed(QRegularExpression(DataBundle::anyGameTagPattern()))
                             .normalizeWhitespace());
                    meta.appendUniqueWord("tags", packageId.segment(2).toString());
                }

                // Apply metadata to the folder representing the package.
                auto &pkgMeta = Package::initializeMetadata(packFolder, packageId);
                pkgMeta.copyMembersFrom(meta);
            }

            // Everythis is complete.
            DENG2_FOR_PUBLIC_AUDIENCE(Download, i)
            {
                i->downloadProgress(self(), 0);
            }
            packageAsset.setState(Asset::Ready);
        }
    }
};

IdgamesPackageInfoFile::IdgamesPackageInfoFile(String const &name)
    : File(name)
    , d(new Impl(this))
{}

void IdgamesPackageInfoFile::setSourceFiles(RemoteFile const &dataFile,
                                            RemoteFile const &descriptionFile)
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

Asset const &IdgamesPackageInfoFile::asset() const
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

IIStream const &IdgamesPackageInfoFile::operator >> (IByteArray &bytes) const
{

    return *this;
}
