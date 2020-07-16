/** @file filesystem.cpp File System.
 *
 * @author Copyright © 2009-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/filesystem.h"

#include "de/app.h"
#include "de/archivefeed.h"
#include "de/directoryfeed.h"
#include "de/dictionaryvalue.h"
#include "de/guard.h"
#include "de/log.h"
#include "de/logbuffer.h"
#include "de/loop.h"
#include "de/nativepath.h"
#include "de/scripting/scriptsystem.h"
#include "de/textvalue.h"
#include "de/ziparchive.h"

#include <condition_variable>

namespace de {

static FileIndex const emptyIndex; // never contains any files

DE_PIMPL_NOREF(FileSystem)
{
    std::mutex              busyMutex;
    int                     busyLevel = 0;
    std::condition_variable busyFinished;

    Record fsModule;

    List<const filesys::IInterpreter *> interpreters;
    
    /// The main index to all files in the file system.
    FileIndex index;

    /// Index of file types. Each entry in the index is another index of names
    /// to file instances.
    typedef Hash<String, FileIndex *> TypeIndex; // owned
    LockableT<TypeIndex> typeIndex;

    Set<FileIndex *> userIndices; // not owned

    /// The root folder of the entire file system.
    std::unique_ptr<Folder> root;

    Impl()
    {
        root.reset(new Folder);

        ScriptSystem::get().addNativeModule("FS", fsModule);
    }

    ~Impl()
    {
        root.reset();

        DE_GUARD(typeIndex);
        typeIndex.value.deleteAll();
        typeIndex.value.clear();
    }

    FileIndex &getTypeIndex(const String &typeName)
    {
        DE_GUARD(typeIndex);
        FileIndex *&idx = typeIndex.value[typeName];
        if (!idx)
        {
            idx = new FileIndex;
        }
        return *idx;
    }

    DE_PIMPL_AUDIENCE(Busy)
};

DE_AUDIENCE_METHOD(FileSystem, Busy)

FileSystem::FileSystem() : d(new Impl)
{}

void FileSystem::addInterpreter(const filesys::IInterpreter &interpreter)
{
    d->interpreters.prepend(&interpreter);
}

void FileSystem::refreshAsync()
{
    // We may need to wait until a previous population is complete.
    Folder::afterPopulation([this] ()
    {
        LOG_AS("FS::refresh");
        d->root->populate(Folder::PopulateAsyncFullTree);
    });
}

Folder &FileSystem::makeFolder(const String &path, FolderCreationBehaviors behavior)
{
    LOG_AS("FS::makeFolder");

    Folder *subFolder = d->root->tryLocate<Folder>(path);
    if (!subFolder)
    {
        // This folder does not exist yet. Let's create it.
        // If the parent folder is missing, it won't be populated yet.
        Folder &parentFolder = makeFolder(path.fileNamePath(), behavior & ~PopulateNewFolder);

        // It is possible that the parent folder has already populated the folder
        // we're looking for.
        if (Folder *folder = parentFolder.tryLocate<Folder>(path.fileName()))
        {
            return *folder;
        }

        // Folders may be interpreted just like any other file; however, they must
        // remain instances derived from Folder.

        // Technically folders could be interpreted, but that would require knowing
        // their source data at this time, and we only know the file name.
        //subFolder = &interpret())->as<Folder>();

        subFolder = new Folder(path.fileName());

        // If parent folder is writable, this will be too.
        if (parentFolder.mode() & File::Write)
        {
            subFolder->setMode(File::Write);
        }

        // Inherit parent's feeds?
        if (behavior & (InheritPrimaryFeed | InheritAllFeeds))
        {
            DE_GUARD(parentFolder);
            for (Feed *parentFeed : parentFolder.feeds())
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

Folder &FileSystem::makeFolderWithFeed(const String &path, Feed *feed,
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
    DE_ASSERT(sourceData != nullptr);

    LOG_AS("FS::interpret");
    try
    {
        for (const filesys::IInterpreter *i : d->interpreters)
        {
            if (auto *file = i->interpretFile(sourceData))
            {
                return file;
            }
        }
    }
    catch (const Error &er)
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

const FileIndex &FileSystem::nameIndex() const
{
    return d->index;
}

int FileSystem::findAll(const String &path, FoundFiles &found) const
{
    LOG_AS("FS::findAll");

    found.clear();
    d->index.findPartialPath(path, found);
    return int(found.size());
}

LoopResult FileSystem::forAll(const String &partialPath, std::function<LoopResult (File &)> func)
{
    FoundFiles files;
    findAll(partialPath, files);
    for (File *f : files)
    {
        if (auto result = func(*f)) return result;
    }
    return LoopContinue;
}

int FileSystem::findAllOfType(const String &typeIdentifier, const String &path, FoundFiles &found) const
{
    LOG_AS("FS::findAllOfType");

    return findAllOfTypes(StringList() << typeIdentifier, path, found);
}

LoopResult FileSystem::forAllOfType(const String &typeIdentifier, const String &path,
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

int FileSystem::findAllOfTypes(StringList typeIdentifiers, const String &path, FoundFiles &found) const
{
    LOG_AS("FS::findAllOfTypes");

    found.clear();
    for (const String &id : typeIdentifiers)
    {
        indexFor(id).findPartialPath(path, found);
    }
    return int(found.size());
}

File &FileSystem::find(const String &path) const
{
    return find<File>(path);
}

void FileSystem::index(File &file)
{
    d->index.maybeAdd(file);

    //qDebug() << "[FS] Indexing" << file.path() << DE_TYPE_NAME(file);

    // Also make an entry in the type index.
    d->getTypeIndex(DE_TYPE_NAME(file)).maybeAdd(file);

    // Also offer to custom indices.
    for (FileIndex *user : d->userIndices)
    {
        user->maybeAdd(file);
    }
}

void FileSystem::deindex(File &file)
{
    d->index.remove(file);
    d->getTypeIndex(DE_TYPE_NAME(file)).remove(file);

    // Also remove from any custom indices.
    for (FileIndex *user : d->userIndices)
    {
        user->remove(file);
    }
}

File &FileSystem::copySerialized(const String &sourcePath, const String &destinationPath,
                                 CopyBehaviors behavior) // static
{
    auto &fs = get();

    Block contents;
    *fs.root().locate<File const>(sourcePath).source() >> contents;

    File *dest = &fs.root().replaceFile(destinationPath);
    *dest << contents;
    dest->flush();

    if (behavior & ReinterpretDestination)
    {
        // We can now reinterpret and populate the contents of the archive.
        //qDebug() << "[FS] Reinterpreting" << dest->path();
        dest = dest->reinterpret();
    }

    if (behavior.testFlag(PopulateDestination) && is<Folder>(dest))
    {
        dest->as<Folder>().populate();
    }

    return *dest;
}

void FileSystem::timeChanged(const Clock &)
{
    // perform time-based processing (indexing/pruning/refreshing)
}

void FileSystem::changeBusyLevel(int increment)
{
    using namespace std;

    bool       notify = false;
    BusyStatus bs     = Idle;
    {
        lock_guard<mutex> g(d->busyMutex);
        const int oldLevel = d->busyLevel;
        d->busyLevel += increment;
        if (d->busyLevel == 0)
        {
            notify = true;
            bs     = Idle;
            d->busyFinished.notify_all();
        }
        else if (oldLevel == 0)
        {
            notify = true;
            bs     = Busy;
        }
    }
    if (notify)
    {
        Loop::mainCall([this, bs]() {
            lock_guard<mutex> g(d->busyMutex);
            // Only notify if the busy level is still up to date.
            if ((bs == Busy && d->busyLevel > 0) ||
                (bs == Idle && d->busyLevel == 0))
            {
                DE_NOTIFY(Busy, i) { i->fileSystemBusyStatusChanged(bs); }
            }
        });
    }
}

int FileSystem::busyLevel() const
{
    std::lock_guard<std::mutex> lk(d->busyMutex);
    return d->busyLevel;
}

void FileSystem::waitForIdle() // static
{
    using namespace std;

    auto &fs = get();
    unique_lock<mutex> lk(fs.d->busyMutex);
    if (fs.d->busyLevel > 0)
    {
        LOG_MSG("Waiting until file system is ready");
        fs.d->busyFinished.wait(lk);
    }
}

const FileIndex &FileSystem::indexFor(const String &typeName) const
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

    DE_GUARD_FOR(d->typeIndex, G);
    for (auto &i : d->typeIndex.value)
    {
        LOG_DEBUG("Index for type '%s' has %i entries") << i.first << i.second->size();

        LOG_AS_STRING(i.first);
        i.second->print();
    }
}

String FileSystem::accessNativeLocation(const NativePath &nativePath, Flags flags) // static
{
    static const String SYS_NATIVE = "/sys/native";
    static const String VAR_MAPPING = "accessNames";

    auto &fs = get();

    Folder &sysNative = fs.makeFolder(SYS_NATIVE);
    if (!sysNative.objectNamespace().hasMember(VAR_MAPPING))
    {
        sysNative.objectNamespace().addDictionary(VAR_MAPPING);
    }

//    String accessPath = SYS_NATIVE;

    // The accessed paths are kept in a dictionary so we'll know when the same path is
    // reaccessed and needs to be replaced.
    DictionaryValue &mapping = sysNative[VAR_MAPPING].value<DictionaryValue>();
    const TextValue key(nativePath);
    if (!mapping.contains(key))
    {
        // Generate a unique access path.
        String name;
        do
        {
            name = Stringf("%04x", Rangei(0, 65536).random());
        } while (sysNative.has(name));
        mapping.setElement(key, new TextValue(name));
    }

    File &f = DirectoryFeed::manuallyPopulateSingleFile(
        nativePath, fs.makeFolder(sysNative.path() / mapping[key].asText()));
    f.setMode(flags);
    return f.path();

    /*    const String path = folders / nativePath.fileNamePath().fileName();
    if (!fs.root().has(path))
    {
        Flags feedFlags = DirectoryFeed::OnlyThisFolder;
        if (flags.testFlag(File::Write)) feedFlags |= DirectoryFeed::AllowWrite;
        fs.makeFolderWithFeed(path,
                              new DirectoryFeed(nativePath.fileNamePath(), feedFlags),
                              Folder::PopulateOnlyThisFolder,
                              FS::DontInheritFeeds | FS::PopulateNewFolder)
                .setMode(flags);
    }*/
    //return path / nativePath.fileName();
}

Folder &FileSystem::root()
{
    return *d->root;
}

const Folder &FileSystem::root() const
{
    return *d->root;
}

Folder &FileSystem::rootFolder() // static
{
    return get().root();
}

FileSystem &FileSystem::get() // static
{
    return App::fileSystem();
}

} // namespace de
