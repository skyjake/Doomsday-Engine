/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2009-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/folder.h"

#include "de/app.h"
#include "de/async.h"
#include "de/directoryfeed.h"
#include "de/filesystem.h"
#include "de/feed.h"
#include "de/guard.h"
#include "de/logbuffer.h"
#include "de/dscript.h"
#include "de/task.h"
#include "de/taskpool.h"
#include "de/unixinfo.h"

namespace de {

FolderPopulationAudience audienceForFolderPopulation; // public

namespace internal {

static TaskPool populateTasks;
static bool     enableBackgroundPopulation = true; // multithreaded folder population

/// Forwards internal folder population notifications to the public audience.
struct PopulationNotifier : DE_OBSERVES(TaskPool, Done)
{
    PopulationNotifier() { populateTasks.audienceForDone() += this; }
    void taskPoolDone(TaskPool &) { notify(); }
    void notify() { DE_NOTIFY_VAR(FolderPopulation, i) i->folderPopulationFinished(); }
};

static PopulationNotifier populationNotifier;

} // namespace internal

DE_PIMPL(Folder)
{
    /// A map of file names to file instances.
    Contents contents;

    /// Feeds provide content for the folder.
    Feeds feeds;

    Impl(Public *i) : Base(i) {}

    void add(File *file)
    {
        DE_ASSERT(file->name().lower() == file->name());
        contents.insert(file->name(), file);
        file->setParent(thisPublic);
    }

    void destroy(String path, File *file)
    {
        Feed *originFeed = file->originFeed();

        // This'll close it and remove it from the index.
        delete file;

        // The origin feed will remove the original data of the file (e.g., the native file).
        if (originFeed)
        {
            originFeed->destroyFile(path);
        }
    }

    List<Folder *> subfolders() const
    {
        DE_GUARD_FOR(self(), G);
        List<Folder *> subs;
        for (Contents::const_iterator i = contents.begin(); i != contents.end(); ++i)
        {
            if (Folder *folder = maybeAs<Folder>(i->second))
            {
                subs << folder;
            }
        }
        return subs;
    }

    static void destroyRecursive(Folder &folder)
    {
        for (Folder *sub : folder.subfolders())
        {
            destroyRecursive(*sub);
        }
        folder.destroyAllFiles();
    }
};

Folder::Folder(const String &name) : File(name), d(new Impl(this))
{
    setStatus(Type::Folder);
    objectNamespace().addSuperRecord(ScriptSystem::builtInClass(DE_STR("Folder")));
}

Folder::~Folder()
{
    DE_GUARD(this);

    DE_NOTIFY(Deletion, i) i->fileBeingDeleted(*this);
    audienceForDeletion().clear();

    deindex();

    // Empty the contents.
    clear();

    // Destroy all feeds that remain.
    while (!d->feeds.isEmpty())
    {
        delete d->feeds.takeLast();
    }
}

String Folder::describe() const
{
    // As a special case, plain native directories should be described as such.
    if (const auto *direcFeed = primaryFeedMaybeAs<DirectoryFeed>())
    {
        return Stringf("directory \"%s\"", direcFeed->nativePath().pretty().c_str());
    }

    String desc;
    if (name().isEmpty())
    {
        desc = "root folder";
    }
    else
    {
        desc = Stringf("folder \"%s\"", name().c_str());
    }

    const String feedDesc = describeFeeds();
    if (!feedDesc.isEmpty())
    {
        desc += Stringf(" (%s)", feedDesc.c_str());
    }

    return desc;
}

String Folder::describeFeeds() const
{
    DE_GUARD(this);

    String desc;

    if (d->feeds.size() == 1)
    {
        desc += Stringf("contains %zu file%s from %s",
                        d->contents.size(),
                        DE_PLURAL_S(d->contents.size()),
                        d->feeds.front()->description().c_str());
    }
    else if (d->feeds.size() > 1)
    {
        desc += Stringf("contains %zu file%s from %zu feed%s",
                        d->contents.size(),
                        DE_PLURAL_S(d->contents.size()),
                        d->feeds.size(),
                        DE_PLURAL_S(d->feeds.size()));

        int n = 0;
        for (auto *i : d->feeds)
        {
            desc += Stringf("; feed #%i is %s", n + 1, i->description().c_str());
            ++n;
        }
    }

    return desc;
}

void Folder::clear()
{
    DE_GUARD(this);

    if (d->contents.empty()) return;

    // Destroy all the file objects.
    for (Contents::iterator i = d->contents.begin(); i != d->contents.end(); ++i)
    {
        i->second->setParent(nullptr);
        delete i->second;
    }
    d->contents.clear();
}

void Folder::populate(PopulationBehaviors behavior)
{
    // Only folders in the file system tree can be populated.
    DE_ASSERT(parent() || this == &FS::get().root());

    if (!behavior.testFlag(DisableIndexing))
    {
        fileSystem().changeBusyLevel(+1);
    }

    LOG_AS("Folder");
    {
        DE_GUARD(this);

        // Prune the existing files first.
        for (auto iter = d->contents.begin(); iter != d->contents.end(); )
        {
            // By default we will NOT prune if there are no feeds attached to the folder.
            // In this case the files were probably created manually, so we shouldn't
            // touch them.
            bool mustPrune = false;

            File *file = iter->second;
            if (file->mode() & DontPrune)
            {
                // Skip this one, it should be kept as-is until manually deleted.
                ++iter;
                continue;
            }

            Feed *originFeed = file->originFeed();

            // If the file has a designated feed, ask it about pruning.
            if (originFeed && originFeed->prune(*file))
            {
                LOG_RES_XVERBOSE("Pruning \"%s\" due to origin feed %s", file->path() << originFeed->description());
                mustPrune = true;
            }
            else if (!originFeed)
            {
                // There is no designated feed, ask all feeds of this folder.
                // If even one of the feeds thinks that the file is out of date,
                // it will be pruned.
                for (auto *f : d->feeds)
                {
                    if (f->prune(*file))
                    {
                        LOG_RES_XVERBOSE("Pruning %s due to non-origin feed %s", file->path() << f->description());
                        mustPrune = true;
                        break;
                    }
                }
            }

            if (mustPrune)
            {               
                // It needs to go.
                file->setParent(nullptr);
                iter = d->contents.erase(iter);
                delete file;
            }
            else
            {
                ++iter;
            }
        }
    }

    auto populationTask = [this, behavior]() {
        Feed::PopulatedFiles newFiles;

        // Populate with new/updated ones.
        for (size_t i = 0; i < d->feeds.size(); ++i)
        {
            newFiles.append(d->feeds.atReverse(i)->populate(*this));
        }

        // Insert and index all new files atomically.
        {
            DE_GUARD(this);
            for (File *i : newFiles)
            {
                if (i)
                {
                    std::unique_ptr<File> file(i);                    
                    if (!d->contents.contains(i->name().lower()))
                    {
                        d->add(file.release());
                        if (!behavior.testFlag(DisableIndexing))
                        {
                            fileSystem().index(*i);
                        }
                    }
                }
            }
            newFiles.clear();
        }

        if (behavior & PopulateFullTree)
        {
            // Call populate on subfolders.
            for (Folder *folder : d->subfolders())
            {
                folder->populate(behavior | DisableNotification);
            }
        }

        if (!behavior.testFlag(DisableIndexing))
        {
            fileSystem().changeBusyLevel(-1);
        }
    };

    if (internal::enableBackgroundPopulation)
    {
        if (behavior & PopulateAsync)
        {
            internal::populateTasks.start(populationTask, TaskPool::MediumPriority);
        }
        else
        {
            populationTask();
        }
    }
    else
    {
        // Only synchronous population is enabled.
        populationTask();

        // Each population gets an individual notification since they're done synchronously.
        // However, only notify once a full hierarchy of populations has finished.
        if (!(behavior & DisableNotification))
        {
            internal::populationNotifier.notify();
        }
    }
}

Folder::Contents Folder::contents() const
{
    DE_GUARD(this);
    return d->contents;
}

LoopResult Folder::forContents(const std::function<LoopResult (String, File &)> &func) const
{
    DE_GUARD(this);

    for (Contents::const_iterator i = d->contents.begin(); i != d->contents.end(); ++i)
    {
        if (auto result = func(i->first, *i->second))
        {
            return result;
        }
    }
    return LoopContinue;
}

List<Folder *> Folder::subfolders() const
{
    return d->subfolders();
}

File &Folder::createFile(const String &newPath, FileCreationBehavior behavior)
{
    String path = newPath.fileNamePath();
    if (!path.empty())
    {
        // Locate the folder where the file will be created in.
        return locate<Folder>(path).createFile(newPath.fileName(), behavior);
    }

    verifyWriteAccess();

    if (behavior == ReplaceExisting && has(newPath))
    {
        try
        {
            destroyFile(newPath);
        }
        catch (const Feed::RemoveError &er)
        {
            LOG_RES_WARNING("Failed to replace %s: existing file could not be removed.\n")
                    << newPath << er.asText();
        }
    }

    // The first feed able to create a file will get the honors.
    for (Feeds::iterator i = d->feeds.begin(); i != d->feeds.end(); ++i)
    {
        File *file = (*i)->createFile(newPath);
        if (file)
        {
            // Allow writing to the new file. Don't prune the file since we will be
            // actively modifying it ourselves (otherwise it would get pruned after
            // a flush modifies the underlying data).
            file->setMode(Write | DontPrune);

            add(file);
            fileSystem().index(*file);
            return *file;
        }
    }

    /// @throw NewFileError All feeds of this folder failed to create a file.
    throw NewFileError("Folder::createFile", "Unable to create new file '" + newPath +
                       "' in " + description());
}

File &Folder::replaceFile(const String &newPath)
{
    return createFile(newPath, ReplaceExisting);
}

void Folder::destroyFile(const String &removePath)
{
    DE_GUARD(this);

    String path = removePath.fileNamePath();
    if (!path.empty())
    {
        // Locate the folder where the file will be removed.
        return locate<Folder>(path).destroyFile(removePath.fileName());
    }

    verifyWriteAccess();

    d->destroy(removePath, &locate<File>(removePath));
}

bool Folder::tryDestroyFile(const String &removePath)
{
    try
    {
        if (has(removePath))
        {
            destroyFile(removePath);
            return true;
        }
    }
    catch (const Error &)
    {
        // This shouldn't happen.
    }
    return false;
}

void Folder::destroyAllFiles()
{
    DE_GUARD(this);

    verifyWriteAccess();

    for (auto &i : d->contents)
    {
        i.second->setParent(nullptr);
        d->destroy(i.second->name(), i.second);
    }
    d->contents.clear();
}

void Folder::destroyAllFilesRecursively()
{
    Impl::destroyRecursive(*this);
}

bool Folder::has(const String &name) const
{
    if (name.isEmpty()) return false;

    // Check if we were given a path rather than just a name.
    String path = name.fileNamePath();
    if (!path.empty())
    {
        Folder *folder = tryLocate<Folder>(path);
        if (folder)
        {
            return folder->has(name.fileName());
        }
        return false;
    }

    DE_GUARD(this);
    return (d->contents.find(name.lower()) != d->contents.end());
}

File &Folder::add(File *file)
{
    DE_ASSERT(file != nullptr);
    DE_ASSERT(!file->name().contains('/'));

    if (has(file->name()))
    {
        /// @throw DuplicateNameError All file names in a folder must be unique.
        throw DuplicateNameError("Folder::add", "Folder cannot contain two files with the same name: '" +
            file->name() + "'");
    }
    DE_GUARD(this);
    d->add(file);
    return *file;
}

File *Folder::remove(const String &name)
{
    DE_GUARD(this);

    DE_ASSERT(d->contents.contains(name.lower()));
    File *removed = d->contents.take(name.lower());
    removed->setParent(nullptr);
    return removed;
}

File *Folder::remove(const char *nameUtf8)
{
    return remove(String(nameUtf8));
}

File *Folder::remove(File &file)
{
    return remove(file.name());
}

const filesys::Node *Folder::tryGetChild(const String &name) const
{
    DE_GUARD(this);

    auto found = d->contents.find(name.lower());
    if (found != d->contents.end())
    {
        return found->second;
    }
    return nullptr;
}

Folder &Folder::root()
{
    return FS::get().root();
}

void Folder::waitForPopulation(WaitBehavior waitBehavior)
{
    if (waitBehavior == OnlyInBackground && App::inMainThread())
    {
        DE_ASSERT(!App::inMainThread());
        throw Error("Folder::waitForPopulation", "Not allowed to block the main thread");
    }
    Time startedAt;
    {
        internal::populateTasks.waitForDone();
    }
    const auto elapsed = startedAt.since();
    if (elapsed > .01)
    {
        LOG_MSG("Waited for %.3f seconds for file system to be ready") << elapsed;
    }
}

AsyncTask *Folder::afterPopulation(std::function<void()> func)
{
    if (!isPopulatingAsync())
    {
        func();
        return nullptr;
    }
    return async([]()
    {
        waitForPopulation();
        return 0;
    },
    [func](int)
    {
        func();
    });
}

bool Folder::isPopulatingAsync()
{
    return !internal::populateTasks.isDone();
}

void Folder::checkDefaultSettings()
{
    String mtEnabled;
    if (App::app().unixInfo().defaults("fs:multithreaded", mtEnabled))
    {
        internal::enableBackgroundPopulation = !ScriptedInfo::isFalse(mtEnabled);
    }
}

const filesys::Node *Folder::tryFollowPath(const PathRef &path) const
{
    // Absolute paths refer to the file system root.
    if (path.isAbsolute())
    {
        return fileSystem().root().tryFollowPath(path.subPath(Rangei(1, path.segmentCount())));
    }

    return Node::tryFollowPath(path);
}

File *Folder::tryLocateFile(const String &path) const
{
    if (const filesys::Node *node = tryFollowPath(Path(path)))
    {
        return const_cast<File *>(maybeAs<File>(node));
    }
    return nullptr;
}

void Folder::attach(Feed *feed)
{
    if (feed)
    {
        DE_GUARD(this);
        d->feeds.push_back(feed);
    }
}

Feed *Folder::detach(Feed &feed)
{
    DE_GUARD(this);

    d->feeds.removeOne(&feed);
    return &feed;
}

void Folder::setPrimaryFeed(Feed &feed)
{
    DE_GUARD(this);

    d->feeds.removeOne(&feed);
    d->feeds.push_front(&feed);
}

Feed *Folder::primaryFeed() const
{
    DE_GUARD(this);

    if (d->feeds.isEmpty()) return nullptr;
    return d->feeds.front();
}

void Folder::clearFeeds()
{
    DE_GUARD(this);

    while (!d->feeds.empty())
    {
        delete detach(*d->feeds.front());
    }
}

Folder::Feeds Folder::feeds() const
{
    DE_GUARD(this);

    return d->feeds;
}

String Folder::contentsAsText() const
{
    List<const File *> files;
    forContents([&files] (String, File &f)
    {
        files << &f;
        return LoopContinue;
    });
    return File::fileListAsText(files);
}

} // namespace de
