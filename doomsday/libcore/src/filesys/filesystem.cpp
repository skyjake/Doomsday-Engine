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
#include "de/ReadWriteLockable"
#include "de/Guard"

namespace de {

static FileSystem::Index const emptyIndex;

DENG2_PIMPL_NOREF(FileSystem), public ReadWriteLockable
{
    /// The main index to all files in the file system.
    FileSystem::Index index;

    /// Index of file types. Each entry in the index is another index of names
    /// to file instances.
    typedef std::map<String, Index> TypeIndex;
    TypeIndex typeIndex;

    /// The root folder of the entire file system.
    Folder root;

    void findInIndex(Index const &idx, String const &path, FoundFiles &found) const
    {
        String baseName = path.fileName().lower();
        String dir      = path.fileNamePath().lower();

        if(!dir.empty() && !dir.beginsWith("/"))
        {
            // Always begin with a slash. We don't want to match partial folder names.
            dir = "/" + dir;
        }

        DENG2_GUARD_READ(this);

        ConstIndexRange range = idx.equal_range(baseName);
        for(Index::const_iterator i = range.first; i != range.second; ++i)
        {
            File *file = i->second;
            if(file->path().fileNamePath().endsWith(dir))
            {
                found.push_back(file);
            }
        }
    }
};

#define DENG2_FS_GUARD_INDEX_R()   DENG2_GUARD_READ_FOR(*d, indexAccessGuardRead)
#define DENG2_FS_GUARD_INDEX_W()   DENG2_GUARD_WRITE_FOR(*d, indexAccessGuardWrite)

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

FileSystem::Index const &FileSystem::nameIndex() const
{
    DENG2_FS_GUARD_INDEX_R();

    return d->index;
}

int FileSystem::findAll(String const &path, FoundFiles &found) const
{
    LOG_AS("FS::findAll");

    found.clear();
    d->findInIndex(d->index, path, found);
    return int(found.size());
}

int FileSystem::findAllOfType(String const &typeIdentifier, String const &path, FoundFiles &found) const
{
    LOG_AS("FS::findAllOfType");

    return findAllOfTypes(StringList() << typeIdentifier, path, found);
}

int FileSystem::findAllOfTypes(StringList const &typeIdentifiers, String const &path, FoundFiles &found) const
{
    LOG_AS("FS::findAllOfTypes");

    found.clear();
    foreach(String const &id, typeIdentifiers)
    {
        d->findInIndex(indexFor(id), path, found);
    }
    return int(found.size());
}

File &FileSystem::find(String const &path) const
{
    return find<File>(path);
}

void FileSystem::index(File &file)
{
    DENG2_FS_GUARD_INDEX_W();

    String const lowercaseName = file.name().lower();

    d->index.insert(IndexEntry(lowercaseName, &file));

    // Also make an entry in the type index.
    Index &indexOfType = d->typeIndex[DENG2_TYPE_NAME(file)];
    indexOfType.insert(IndexEntry(lowercaseName, &file));
}

static void removeFromIndex(FileSystem::Index &idx, File &file)
{
    if(idx.empty())
    {
        return;
    }       

    // Look up the ones that might be this file.
    FileSystem::IndexRange range = idx.equal_range(file.name().lower());

    for(FileSystem::Index::iterator i = range.first; i != range.second; ++i)
    {
        if(i->second == &file)
        {
            // This is the one to deindex.
            idx.erase(i);
            break;
        }
    }
}

void FileSystem::deindex(File &file)
{
    DENG2_FS_GUARD_INDEX_W();

    removeFromIndex(d->index, file);
    removeFromIndex(d->typeIndex[DENG2_TYPE_NAME(file)], file);
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

FileSystem::Index const &FileSystem::indexFor(String const &typeName) const
{
    DENG2_FS_GUARD_INDEX_R();

    Instance::TypeIndex::const_iterator found = d->typeIndex.find(typeName);
    if(found != d->typeIndex.end())
    {
        return found->second;
    }
    return emptyIndex;
    /*
    /// @throw UnknownTypeError No files of type @a typeName have been indexed.
    throw UnknownTypeError("FS::indexFor", "No files of type '" + typeName + "' have been indexed");
    */
}

void FileSystem::printIndex()
{
    DENG2_FS_GUARD_INDEX_R();

    LOG_DEBUG("Main FS index has %i entries") << d->index.size();

    for(Index::iterator i = d->index.begin(); i != d->index.end(); ++i)
    {
        LOG_TRACE("\"%s\": ") << i->first << i->second->description();
    }

    for(Instance::TypeIndex::iterator k = d->typeIndex.begin(); k != d->typeIndex.end(); ++k)
    {
        LOG_DEBUG("Index for type '%s' has %i entries") << k->first << k->second.size();

        LOG_AS_STRING(k->first);
        for(Index::iterator i = k->second.begin(); i != k->second.end(); ++i)
        {
            LOG_TRACE("\"%s\": ") << i->first << i->second->description();
        }
    }
}

Folder &FileSystem::root()
{
    return d->root;
}

} // namespace de
