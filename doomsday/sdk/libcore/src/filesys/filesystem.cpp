/** @file filesystem.cpp File System.
 *
 * @author Copyright © 2009-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright © 2013 Daniel Swanson <danij@dengine.net>
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

#include "de/FS"

#include "de/App"
#include "de/ArchiveFeed"
#include "de/ArchiveFolder"
#include "de/DirectoryFeed"
#include "de/Guard"
#include "de/LibraryFile"
#include "de/Log"
#include "de/LogBuffer"
#include "de/NativePath"
#include "de/ZipArchive"

#include <QHash>

namespace de {

static FileIndex const emptyIndex; // never contains any files

DENG2_PIMPL_NOREF(FileSystem)
{
    QList<filesys::IInterpreter const *> interpreters;

    /// The main index to all files in the file system.
    FileIndex index;

    /// Index of file types. Each entry in the index is another index of names
    /// to file instances.
    typedef QHash<String, FileIndex *> TypeIndex; // owned
    TypeIndex typeIndex;

    QSet<FileIndex *> userIndices; // not owned

    /// The root folder of the entire file system.
    std::unique_ptr<Folder> root;

    Impl()
    {
        root.reset(new Folder);
    }

    ~Impl()
    {
        root.reset();
        qDeleteAll(typeIndex.values());
        typeIndex.clear();
    }

    FileIndex &getTypeIndex(String const &typeName)
    {
        FileIndex *&idx = typeIndex[typeName];
        if (!idx)
        {
            idx = new FileIndex;
        }
        return *idx;
    }
};

FileSystem::FileSystem() : d(new Impl)
{}

void FileSystem::addInterpreter(filesys::IInterpreter const &interpreter)
{
    d->interpreters.prepend(&interpreter);
}

void FileSystem::refresh()
{
    if (Folder::isPopulatingAsync())
    {
        qDebug() << "FileSystem has to wait for previous population to finish...";
    }

    // Wait for a previous refresh to finish.
    Folder::waitForPopulation();

    LOG_AS("FS::refresh");

    //Time startedAt;
    d->root->populate(Folder::PopulateAsyncFullTree);

    /*LOGDEV_RES_VERBOSE("Completed in %.2f seconds") << startedAt.since();
    printIndex();*/
}

Folder &FileSystem::makeFolder(String const &path, FolderCreationBehaviors behavior)
{
    LOG_AS("FS::makeFolder");

    Folder *subFolder = d->root->tryLocate<Folder>(path);
    if (!subFolder)
    {
        // This folder does not exist yet. Let's create it.
        Folder &parentFolder = makeFolder(path.fileNamePath(), behavior);

        // Folders may be interpreted just like any other file; however, they must
        // remain instances derived from Folder.
        subFolder = &interpret(new Folder(path.fileName()))->as<Folder>();

        // If parent folder is writable, this will be too.
        if (parentFolder.mode() & File::Write)
        {
            subFolder->setMode(File::Write);
        }

        // Inherit parent's feeds?
        if (behavior & (InheritPrimaryFeed | InheritAllFeeds))
        {
            DENG2_GUARD(parentFolder);
            foreach (Feed *parentFeed, parentFolder.feeds())
            {
                Feed *feed = parentFeed->newSubFeed(subFolder->name());
                if (!feed) continue; // Check next one instead.

                LOGDEV_RES_XVERBOSE_DEBUGONLY("Creating subfeed \"%s\" from %s",
                                             subFolder->name() << parentFeed->description());

                subFolder->attach(feed);

                if (!behavior.testFlag(InheritAllFeeds)) break;
            }
        }

        parentFolder.add(subFolder);
        index(*subFolder);

        if (behavior.testFlag(PopulateNewFolder))
        {
            // Populate the new folder.
            subFolder->populate();
        }
    }
    return *subFolder;
}

Folder &FileSystem::makeFolderWithFeed(String const &path, Feed *feed,
                                       Folder::PopulationBehavior populationBehavior,
                                       FolderCreationBehaviors behavior)
{
    makeFolder(path.fileNamePath(), behavior);

    Folder &folder = makeFolder(path, DontInheritFeeds /* we have a specific feed to attach */);
    folder.clear();
    folder.clearFeeds();
    folder.attach(feed);
    if (behavior & PopulateNewFolder)
    {
        folder.populate(populationBehavior);
    }
    return folder;
}

File *FileSystem::interpret(File *sourceData)
{
    DENG2_ASSERT(sourceData != nullptr);

    LOG_AS("FS::interpret");
    try
    {
        for (filesys::IInterpreter const *i : d->interpreters)
        {
            if (auto *file = i->interpretFile(sourceData))
            {
                return file;
            }
        }
    }
    catch (Error const &er)
    {
        LOG_RES_ERROR("Failed to interpret contents of %s: %s")
                << sourceData->description() << er.asText();

        // The error is one we don't know how to handle. We were given
        // responsibility of the source file, so it has to be deleted.
        delete sourceData;

        throw;
    }
    return sourceData;
}

FileIndex const &FileSystem::nameIndex() const
{
    return d->index;
}

int FileSystem::findAll(String const &path, FoundFiles &found) const
{
    LOG_AS("FS::findAll");

    found.clear();
    d->index.findPartialPath(path, found);
    return int(found.size());
}

LoopResult FileSystem::forAll(String const &partialPath, std::function<LoopResult (File &)> func)
{
    FoundFiles files;
    findAll(partialPath, files);
    for (File *f : files)
    {
        if (auto result = func(*f)) return result;
    }
    return LoopContinue;
}

int FileSystem::findAllOfType(String const &typeIdentifier, String const &path, FoundFiles &found) const
{
    LOG_AS("FS::findAllOfType");

    return findAllOfTypes(StringList() << typeIdentifier, path, found);
}

LoopResult FileSystem::forAllOfType(String const &typeIdentifier, String const &path,
                                    std::function<LoopResult (File &)> func)
{
    FoundFiles files;
    findAllOfType(typeIdentifier, path, files);
    for (File *f : files)
    {
        if (auto result = func(*f)) return result;
    }
    return LoopContinue;
}

int FileSystem::findAllOfTypes(StringList const &typeIdentifiers, String const &path, FoundFiles &found) const
{
    LOG_AS("FS::findAllOfTypes");

    found.clear();
    foreach (String const &id, typeIdentifiers)
    {
        indexFor(id).findPartialPath(path, found);
    }
    return int(found.size());
}

File &FileSystem::find(String const &path) const
{
    return find<File>(path);
}

void FileSystem::index(File &file)
{
    d->index.maybeAdd(file);

    //qDebug() << "[FS] Indexing" << file.path() << DENG2_TYPE_NAME(file);

    // Also make an entry in the type index.
    d->getTypeIndex(DENG2_TYPE_NAME(file)).maybeAdd(file);

    // Also offer to custom indices.
    foreach (FileIndex *user, d->userIndices)
    {
        user->maybeAdd(file);
    }
}

void FileSystem::deindex(File &file)
{
    d->index.remove(file);
    d->getTypeIndex(DENG2_TYPE_NAME(file)).remove(file);

    // Also remove from any custom indices.
    foreach (FileIndex *user, d->userIndices)
    {
        user->remove(file);
    }
}

File &FileSystem::copySerialized(String const &sourcePath, String const &destinationPath,
                                 CopyBehaviors behavior)
{
    Block contents;
    *root().locate<File const>(sourcePath).source() >> contents;

    File *dest = &root().replaceFile(destinationPath);
    *dest << contents;
    dest->flush();

    if (behavior & ReinterpretDestination)
    {
        // We can now reinterpret and populate the contents of the archive.
        //qDebug() << "[FS] Reinterpreting" << dest->path();
        dest = dest->reinterpret();
    }

    if (behavior.testFlag(PopulateDestination) && dest->is<Folder>())
    {
        dest->as<Folder>().populate();
    }

    return *dest;
}

void FileSystem::timeChanged(Clock const &)
{
    // perform time-based processing (indexing/pruning/refreshing)
}

FileIndex const &FileSystem::indexFor(String const &typeName) const
{
    return d->getTypeIndex(typeName);
}

void FileSystem::addUserIndex(FileIndex &userIndex)
{
    d->userIndices.insert(&userIndex);
}

void FileSystem::removeUserIndex(FileIndex &userIndex)
{
    d->userIndices.remove(&userIndex);
}

void FileSystem::printIndex()
{
    if (!LogBuffer::get().isEnabled(LogEntry::Generic | LogEntry::Dev | LogEntry::Verbose))
        return;

    LOG_DEBUG("Main FS index has %i entries") << d->index.size();
    d->index.print();

    DENG2_FOR_EACH_CONST(Impl::TypeIndex, i, d->typeIndex)
    {
        LOG_DEBUG("Index for type '%s' has %i entries") << i.key() << i.value()->size();

        LOG_AS_STRING(i.key());
        i.value()->print();
    }
}

String FileSystem::accessNativeLocation(NativePath const &nativePath, File::Flags flags)
{
    String const folders = "/sys/folders";

    makeFolder(folders);

    String const path = folders / nativePath.fileNamePath().fileName();
    if (!root().has(path))
    {
        DirectoryFeed::Flags feedFlags = DirectoryFeed::OnlyThisFolder;
        if (flags.testFlag(File::Write)) feedFlags |= DirectoryFeed::AllowWrite;
        makeFolderWithFeed(path,
                           new DirectoryFeed(nativePath.fileNamePath(), feedFlags),
                           Folder::PopulateOnlyThisFolder,
                           FS::DontInheritFeeds | FS::PopulateNewFolder)
                .setMode(flags);
    }
    return path / nativePath.fileName();
}

Folder &FileSystem::root()
{
    return *d->root;
}

Folder const &FileSystem::root() const
{
    return *d->root;
}

FileSystem &FileSystem::get() // static
{
    return App::fileSystem();
}

} // namespace de
