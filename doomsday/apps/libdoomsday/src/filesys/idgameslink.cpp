/** @file idgameslink.cpp
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "doomsday/filesys/idgameslink.h"
#include "doomsday/filesys/idgamespackageinfofile.h"
#include "doomsday/DataBundle"

#include <de/Async>
#include <de/FileSystem>
#include <de/RemoteFile>
#include <de/data/gzip.h>

#include <QRegularExpression>
#include <QUrl>

using namespace de;

static String const DOMAIN_IDGAMES("idgames");
static String const CATEGORY_LEVELS("levels");
static String const CATEGORY_MUSIC ("music");
static String const CATEGORY_SOUNDS("sounds");
static String const CATEGORY_THEMES("themes");

DE_PIMPL(IdgamesLink)
{
    struct PackageIndexEntry : public PathTree::Node
    {
        FileEntry const *file = nullptr;
        Version version;

        PackageIndexEntry(PathTree::NodeArgs const &args) : Node(args) {}

        String descriptionPath() const
        {
            return file->path().toString().fileNameAndPathWithoutExtension() + ".txt";
        }
    };
    PathTreeT<PackageIndexEntry> packageIndex;

    String localRootPath;

    Impl(Public *i) : Base(i)
    {
        localRootPath = "/remote/" + QUrl(self().address()).host();
    }

    String packageIdentifierForFileEntry(FileEntry const &entry) const
    {
        if (entry.name().fileNameExtension() == QStringLiteral(".zip"))
        {
            Path const path = entry.path();
            String id = QString("%1_%2")
                    .arg(DataBundle::cleanIdentifier(path.fileName().fileNameWithoutExtension()))
                    .arg(DataBundle::versionFromTimestamp(entry.modTime));

            // Remove the hour:minute part.
            id.truncate(id.size() - 5);

            if (path.segment(1) == CATEGORY_MUSIC ||
                path.segment(1) == CATEGORY_SOUNDS ||
                path.segment(1) == CATEGORY_THEMES)
            {
                return String("%1.%2.%3")
                        .arg(DOMAIN_IDGAMES)
                        .arg(path.segment(1))
                        .arg(id);
            }
            if (path.segment(1) == CATEGORY_LEVELS)
            {
                String subset;
                if      (path.segment(3) == QStringLiteral("deathmatch")) subset = QStringLiteral("deathmatch.");
                else if (path.segment(3) == QStringLiteral("megawads"))   subset = QStringLiteral("megawads.");
                return String("%1.%2.%3.%4%5")
                        .arg(DOMAIN_IDGAMES)
                        .arg(CATEGORY_LEVELS)
                        .arg(path.segment(2))
                        .arg(subset)
                        .arg(id);
            }
            return String("%1.%2").arg(DOMAIN_IDGAMES).arg(id);
        }
        else
        {
            return String();
        }
    }

    void buildPackageIndex()
    {
        packageIndex.clear();

        PathTreeIterator<FileTree> iter(self().fileTree().leafNodes());
        while (iter.hasNext())
        {
            auto const &fileEntry = iter.next();
            if (String pkg = packageIdentifierForFileEntry(fileEntry))
            {
                auto const id_ver = Package::split(pkg);
                auto &pkgEntry = packageIndex.insert(DotPath(id_ver.first));
                pkgEntry.file    = &fileEntry;
                pkgEntry.version = id_ver.second;
            }
        }

        qDebug() << "idgames package index has" << packageIndex.size() << "entries";
    }

    PackageIndexEntry const *findPackage(String const &packageId) const
    {
        auto const id_ver = Package::split(packageId);
        if (auto *found = packageIndex.tryFind(DotPath(id_ver.first),
                                               PathTree::MatchFull | PathTree::NoBranch))
        {
            if (!id_ver.second.isValid() || found->version == id_ver.second)
            {
                return found;
            }
        }
        return nullptr;
    }

    RemoteFile &makeRemoteFile(Folder &folder, String const &remotePath, Block const &remoteMetaId)
    {
        auto *file = new RemoteFile(remotePath.fileName(),
                                    remotePath,
                                    remoteMetaId,
                                    self().address());
        folder.add(file);
        FS::get().index(*file);
        return *file;
    }
};

IdgamesLink::IdgamesLink(String const &address)
    : filesys::WebHostedLink(address, "ls-laR.gz")
    , d(new Impl(this))
{}

void IdgamesLink::parseRepositoryIndex(QByteArray data)
{
    // This may be a long list, so let's do it in a background thread.
    // The link will be marked connected only after the data has been parsed.

    scope() += async([this, data] () -> String
    {
        Block const listing = gDecompress(data);
        QTextStream is(listing, QIODevice::ReadOnly);
        is.setCodec("UTF-8");
        QRegularExpression const reDir("^\\.?(.*):$");
        QRegularExpression const reTotal("^total\\s+\\d+$");
        QRegularExpression const reFile("^(-|d)[-rwxs]+\\s+\\d+\\s+\\w+\\s+\\w+\\s+"
                                        "(\\d+)\\s+(\\w+\\s+\\d+\\s+[0-9:]+)\\s+(.*)$",
                                        QRegularExpression::CaseInsensitiveOption);
        String currentPath;
        bool ignore = false;
        QRegularExpression const reIncludedPaths("^/(levels|music|sounds|themes)");

        std::unique_ptr<FileTree> tree(new FileTree);
        while (!is.atEnd())
        {
            if (String const line = is.readLine().trimmed())
            {
                if (!currentPath)
                {
                    // This should be a directory path.
                    auto match = reDir.match(line);
                    if (match.hasMatch())
                    {
                        currentPath = match.captured(1);
                        //qDebug() << "[WebRepositoryLink] Parsing path:" << currentPath;

                        ignore = !reIncludedPaths.match(currentPath).hasMatch();
                    }
                }
                else if (!ignore && reTotal.match(line).hasMatch())
                {
                    // Ignore directory sizes.
                }
                else if (!ignore)
                {
                    auto match = reFile.match(line);
                    if (match.hasMatch())
                    {
                        bool const isFolder = (match.captured(1) == QStringLiteral("d"));
                        if (!isFolder)
                        {
                            String const name = match.captured(4);
                            if (name.startsWith(QChar('.')) || name.contains(" -> "))
                                continue;

                            auto &entry = tree->insert((currentPath / name).toLower());
                            entry.size = match.captured(2).toULongLong(nullptr, 10);;
                            entry.modTime = Time::fromText(match.captured(3), Time::UnixLsStyleDateTime);
                        }
                    }
                }
            }
            else
            {
                currentPath.clear();
            }
        }
        qDebug() << "idgames file tree contains" << tree->size() << "entries";
        setFileTree(tree.release());
        return String();
    },
    [this] (String const &errorMessage)
    {
        if (!errorMessage)
        {
            wasConnected();
        }
        else
        {
            handleError("Failed to parse directory listing: " + errorMessage);
            wasDisconnected();
        }
});
}

StringList IdgamesLink::categoryTags() const
{
    return StringList({ CATEGORY_LEVELS,
                        CATEGORY_MUSIC,
                        CATEGORY_SOUNDS,
                        CATEGORY_THEMES });
}

LoopResult IdgamesLink::forPackageIds(std::function<LoopResult (String const &)> func) const
{
    PathTreeIterator<Impl::PackageIndexEntry> iter(d->packageIndex.leafNodes());
    while (iter.hasNext())
    {
        if (auto result = func(iter.next().path('.')))
        {
            return result;
        }
    }
    return LoopContinue;
}

String IdgamesLink::findPackagePath(String const &packageId) const
{
    if (auto *found = d->findPackage(packageId))
    {
        return found->file->path();
    }
    return String();
}

filesys::Link *IdgamesLink::construct(String const &address)
{
    if ((address.startsWith("http:") || address.startsWith("https:")) &&
        !address.contains("dengine.net"))
    {
        return new IdgamesLink(address);
    }
    return nullptr;
}

File *IdgamesLink::populateRemotePath(String const &packageId,
                                      filesys::RepositoryPath const &path) const
{
    DE_ASSERT(path.link == this);

    auto *pkgEntry = d->findPackage(packageId);
    if (!pkgEntry)
    {
        DE_ASSERT(pkgEntry);
        return nullptr;
    }

    auto &pkgFolder = FS::get().makeFolder(path.localPath, FS::DontInheritFeeds);

    // The main data file of the package.
    auto &dataFile = d->makeRemoteFile(pkgFolder,
                                       pkgEntry->file->path(),
                                       pkgEntry->file->metaId(*this));
    dataFile.setStatus(File::Status(pkgEntry->file->size, pkgEntry->file->modTime));

    // Additional description.
    auto &txtFile = d->makeRemoteFile(pkgFolder,
                                      pkgEntry->descriptionPath(),
                                      md5Hash(address(), pkgEntry->descriptionPath(), pkgEntry->file->modTime));
    if (auto const *txtEntry = findFile(pkgEntry->descriptionPath()))
    {
        txtFile.setStatus(File::Status(txtEntry->size, txtEntry->modTime));
    }

    auto *infoFile = new IdgamesPackageInfoFile("info.dei");
    infoFile->setSourceFiles(dataFile, txtFile);
    pkgFolder.add(infoFile);
    FS::get().index(*infoFile);

    return &pkgFolder;
}

void IdgamesLink::setFileTree(FileTree *tree)
{
    WebHostedLink::setFileTree(tree);
    d->buildPackageIndex();
}
