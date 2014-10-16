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
#include "de/LibraryFile"
#include "de/DirectoryFeed"
#include "de/ArchiveFeed"
#include "de/game/SavedSession"
#include "de/NativePath"
#include "de/ArchiveFolder"
#include "de/ZipArchive"
#include "de/Log"
#include "de/LogBuffer"
#include "de/Guard"

namespace de {

static FileIndex const emptyIndex; // never contains any files

DENG2_PIMPL_NOREF(FileSystem)
{
    /// The main index to all files in the file system.
    FileIndex index;

    /// Index of file types. Each entry in the index is another index of names
    /// to file instances.
    typedef QMap<String, FileIndex *> TypeIndex; // owned
    TypeIndex typeIndex;

    QSet<FileIndex *> userIndices; // not owned

    /// The root folder of the entire file system.
    Folder root;

    ~Instance()
    {
        qDeleteAll(typeIndex.values());
        typeIndex.clear();
    }
};

FileSystem::FileSystem() : d(new Instance)
{}

void FileSystem::refresh()
{
    LOG_AS("FS::refresh");

    Time startedAt;
    d->root.populate();

    LOGDEV_RES_VERBOSE("Completed in %.2f seconds") << startedAt.since();

    printIndex();
}

Folder &FileSystem::makeFolder(String const &path, FolderCreationBehaviors behavior)
{
    LOG_AS("FS::makeFolder");

    Folder *subFolder = d->root.tryLocate<Folder>(path);
    if(!subFolder)
    {
        // This folder does not exist yet. Let's create it.
        Folder &parentFolder = makeFolder(path.fileNamePath(), behavior);
        subFolder = new Folder(path.fileName());

        // If parent folder is writable, this will be too.
        if(parentFolder.mode() & File::Write)
        {
            subFolder->setMode(File::Write);
        }

        // Inherit parent's feeds?
        if(behavior & (InheritPrimaryFeed | InheritAllFeeds))
        {
            DENG2_GUARD(parentFolder);
            DENG2_FOR_EACH_CONST(Folder::Feeds, i, parentFolder.feeds())
            {
                Feed *feed = (*i)->newSubFeed(subFolder->name());
                if(!feed) continue; // Check next one instead.

                LOGDEV_RES_XVERBOSE_DEBUGONLY("Creating subfeed \"%s\" from %s",
                                             subFolder->name() << (*i)->description());

                subFolder->attach(feed);

                if(!behavior.testFlag(InheritAllFeeds)) break;
            }
        }

        parentFolder.add(subFolder);
        index(*subFolder);

        if(behavior.testFlag(PopulateNewFolder))
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
    if(behavior & PopulateNewFolder)
    {
        folder.populate(populationBehavior);
    }
    return folder;
}

File *FileSystem::interpret(File *sourceData)
{
    LOG_AS("FS::interpret");

    /// @todo  One should be able to define new interpreters dynamically.

    try
    {
        if(LibraryFile::recognize(*sourceData))
        {
            LOG_RES_VERBOSE("Interpreted ") << sourceData->description() << " as a shared library";

            // It is a shared library intended for Doomsday.
            return new LibraryFile(sourceData);
        }
        if(ZipArchive::recognize(*sourceData))
        {
            try
            {
                // It is a ZIP archive: we will represent it as a folder.
                std::auto_ptr<ArchiveFolder> package;

                if(sourceData->name().fileNameExtension() == ".save")
                {
                    /// @todo fixme: Don't assume this is a save package.
                    LOG_RES_VERBOSE("Interpreted %s as a SavedSession") << sourceData->description();
                    package.reset(new game::SavedSession(*sourceData, sourceData->name()));
                }
                else
                {
                    LOG_RES_VERBOSE("Interpreted %s as a ZIP format archive") << sourceData->description();
                    package.reset(new ArchiveFolder(*sourceData, sourceData->name()));
                }

                // Archive opened successfully, give ownership of the source to the folder.
                package->setSource(sourceData);
                return package.release();
            }
            catch(Archive::FormatError const &)
            {
                // Even though it was recognized as an archive, the file
                // contents may still prove to be corrupted.
                LOG_RES_WARNING("Archive in %s is invalid") << sourceData->description();
            }
            catch(IByteArray::OffsetError const &)
            {
                LOG_RES_WARNING("Archive in %s is truncated") << sourceData->description();
            }
            catch(IIStream::InputError const &er)
            {
                LOG_RES_WARNING("Failed to read %s") << sourceData->description();
                LOGDEV_RES_WARNING("%s") << er.asText();
            }
        }
    }
    catch(Error const &err)
    {
        LOG_RES_ERROR("") << err.asText();

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

Iteration FileSystem::forAll(String const &partialPath, std::function<Iteration (File &)> func)
{
    FoundFiles files;
    findAll(partialPath, files);
    for(File *f : files)
    {
        if(func(*f) == IterAbort) return IterAbort;
    }
    return IterContinue;
}

int FileSystem::findAllOfType(String const &typeIdentifier, String const &path, FoundFiles &found) const
{
    LOG_AS("FS::findAllOfType");

    return findAllOfTypes(StringList() << typeIdentifier, path, found);
}

Iteration FileSystem::forAllOfType(String const &typeIdentifier, String const &path,
                                   std::function<Iteration (File &)> func)
{
    FoundFiles files;
    findAllOfType(typeIdentifier, path, files);
    for(File *f : files)
    {
        if(func(*f) == IterAbort) return IterAbort;
    }
    return IterContinue;
}

int FileSystem::findAllOfTypes(StringList const &typeIdentifiers, String const &path, FoundFiles &found) const
{
    LOG_AS("FS::findAllOfTypes");

    found.clear();
    foreach(String const &id, typeIdentifiers)
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

    // Also make an entry in the type index.
    String const typeName = DENG2_TYPE_NAME(file);
    if(!d->typeIndex.contains(typeName))
    {
        d->typeIndex.insert(typeName, new FileIndex);
    }
    d->typeIndex[typeName]->maybeAdd(file);

    // Also offer to custom indices.
    foreach(FileIndex *user, d->userIndices)
    {
        user->maybeAdd(file);
    }
}

void FileSystem::deindex(File &file)
{
    d->index.remove(file);

    String const typeName = DENG2_TYPE_NAME(file);
    if(d->typeIndex.contains(typeName))
    {
        d->typeIndex[typeName]->remove(file);
    }

    // Also remove from any custom indices.
    foreach(FileIndex *user, d->userIndices)
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

    if(behavior & ReinterpretDestination)
    {
        // We can now reinterpret and populate the contents of the archive.
        dest = dest->reinterpret();
    }

    if(behavior.testFlag(PopulateDestination) && dest->is<Folder>())
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
    Instance::TypeIndex::const_iterator found = d->typeIndex.constFind(typeName);
    if(found != d->typeIndex.constEnd())
    {
        return *found.value();
    }
    return emptyIndex;
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
    if(!LogBuffer::get().isEnabled(LogEntry::Generic | LogEntry::Dev | LogEntry::Verbose))
        return;

    LOG_DEBUG("Main FS index has %i entries") << d->index.size();
    d->index.print();

    DENG2_FOR_EACH_CONST(Instance::TypeIndex, i, d->typeIndex)
    {
        LOG_DEBUG("Index for type '%s' has %i entries") << i.key() << i.value()->size();

        LOG_AS_STRING(i.key());
        i.value()->print();
    }
}

Folder &FileSystem::root()
{
    return d->root;
}

Folder const &FileSystem::root() const
{
    return d->root;
}

} // namespace de
