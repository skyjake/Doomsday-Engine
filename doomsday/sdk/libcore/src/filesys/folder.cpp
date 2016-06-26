/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2009-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/Folder"
#include "de/Feed"
#include "de/FS"
#include "de/NumberValue"
#include "de/Log"
#include "de/Guard"
#include "de/TaskPool"
#include "de/Task"

namespace de {

FolderPopulationAudience audienceForFolderPopulation; // public

namespace internal
{
    static TaskPool populateTasks;

    /// Forwards internal folder population notifications to the public audience.
    struct PopulationNotifier : DENG2_OBSERVES(TaskPool, Done)
    {
        PopulationNotifier() {
            populateTasks.audienceForDone() += this;
        }
        void taskPoolDone(TaskPool &) {
            DENG2_FOR_AUDIENCE(FolderPopulation, i) i->folderPopulationFinished();
        }
    };
    static PopulationNotifier populationNotifier;
}

DENG2_PIMPL(Folder), public Lockable
{
    /// A map of file names to file instances.
    Contents contents;

    /// Feeds provide content for the folder.
    Feeds feeds;

    Instance(Public *i) : Base(i) {}

    void add(File *file)
    {
        contents.insert(file->name().toLower(), file);
        file->setParent(&self);
    }

    QList<Folder *> subfolders() const
    {
        DENG2_GUARD(this);
        QList<Folder *> subs;
        for (Contents::const_iterator i = contents.begin(); i != contents.end(); ++i)
        {
            if (Folder *folder = i.value()->maybeAs<Folder>())
            {
                subs << folder;
            }
        }
        return subs;
    }
};

Folder::Folder(String const &name) : File(name), d(new Instance(this))
{
    setStatus(Status::FOLDER);

    // Standard info.
    objectNamespace().add(new Variable("contentSize",
                                       new Accessor(*this, Accessor::CONTENT_SIZE),
                                       Accessor::VARIABLE_MODE));
}

Folder::~Folder()
{
    DENG2_GUARD(this);

    DENG2_FOR_AUDIENCE2(Deletion, i) i->fileBeingDeleted(*this);
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
    String desc;
    if (name().isEmpty())
    {
        desc = "root folder";
    }
    else
    {
        desc = String("folder \"%1\"").arg(name());
    }

    String const feedDesc = describeFeeds();
    if (!feedDesc.isEmpty())
    {
        desc += String(" (%1)").arg(feedDesc);
    }

    return desc;
}

String Folder::describeFeeds() const
{
    DENG2_GUARD(d);

    String desc;

    if (d->feeds.size() == 1)
    {
        desc += String("contains %1 file%2 from %3")
                .arg(d->contents.size())
                .arg(DENG2_PLURAL_S(d->contents.size()))
                .arg(d->feeds.front()->description());
    }
    else if (d->feeds.size() > 1)
    {
        desc += String("contains %1 file%2 from %3 feed%4")
                .arg(d->contents.size())
                .arg(DENG2_PLURAL_S(d->contents.size()))
                .arg(d->feeds.size())
                .arg(DENG2_PLURAL_S(d->feeds.size()));

        int n = 0;
        DENG2_FOR_EACH_CONST(Feeds, i, d->feeds)
        {
            desc += String("; feed #%2 is %3")
                    .arg(n + 1)
                    .arg((*i)->description());
            ++n;
        }
    }

    return desc;
}

void Folder::clear()
{
    DENG2_GUARD(d);

    if (d->contents.empty()) return;

    // Destroy all the file objects.
    for (Contents::iterator i = d->contents.begin(); i != d->contents.end(); ++i)
    {
        i.value()->setParent(0);
        delete i.value();
    }
    d->contents.clear();
}

void Folder::populate(PopulationBehaviors behavior)
{
    LOG_AS("Folder");
    {
        DENG2_GUARD(d);

        // Prune the existing files first.
        QMutableMapIterator<String, File *> iter(d->contents);
        while (iter.hasNext())
        {
            iter.next();

            // By default we will NOT prune if there are no feeds attached to the folder.
            // In this case the files were probably created manually, so we shouldn't
            // touch them.
            bool mustPrune = false;

            File *file = iter.value();
            Feed *originFeed = file->originFeed();

            // If the file has a designated feed, ask it about pruning.
            if (originFeed && originFeed->prune(*file))
            {
                LOG_RES_XVERBOSE("Pruning \"%s\" due to origin feed %s") << file->path() << originFeed->description();
                mustPrune = true;
            }
            else if (!originFeed)
            {
                // There is no designated feed, ask all feeds of this folder.
                // If even one of the feeds thinks that the file is out of date,
                // it will be pruned.
                for (Feeds::iterator f = d->feeds.begin(); f != d->feeds.end(); ++f)
                {
                    if ((*f)->prune(*file))
                    {
                        LOG_RES_XVERBOSE("Pruning %s due to non-origin feed %s") << file->path() << (*f)->description();
                        mustPrune = true;
                        break;
                    }
                }
            }

            if (mustPrune)
            {
                // It needs to go.
                file->setParent(nullptr);
                iter.remove();
                delete file;
            }
        }
    }

    internal::populateTasks.start([this, behavior] ()
    {
        Feed::PopulatedFiles newFiles;

        // Populate with new/updated ones.
        for (int i = d->feeds.size() - 1; i >= 0; --i)
        {
            newFiles.append(d->feeds.at(i)->populate(*this));
        }

        // Insert and index all new files atomically.
        {
            DENG2_GUARD(d);
            for (File *i : newFiles)
            {
                if (!i) continue;

                std::unique_ptr<File> file(i);
                if (!d->contents.contains(i->name().toLower()))
                {
                    d->add(file.release());
                    fileSystem().index(*i);
                }
            }
            newFiles.clear();
        }

        if (behavior & PopulateFullTree)
        {
            // Call populate on subfolders.
            for (Folder *folder : d->subfolders())
            {
                folder->populate(behavior);
            }
        }
    }
    , TaskPool::MediumPriority);

    if (!(behavior & PopulateAsync))
    {
        waitForPopulation();
    }
}

Folder::Contents Folder::contents() const
{
    DENG2_GUARD(d);

    return d->contents;
}

LoopResult Folder::forContents(std::function<LoopResult (String, File &)> func) const
{
    DENG2_GUARD(d);

    for (Contents::const_iterator i = d->contents.constBegin(); i != d->contents.constEnd(); ++i)
    {
        if (auto result = func(i.key(), *i.value()))
        {
            return result;
        }
    }
    return LoopContinue;
}

File &Folder::newFile(String const &newPath, FileCreationBehavior behavior)
{
    String path = newPath.fileNamePath();
    if (!path.empty())
    {
        // Locate the folder where the file will be created in.
        return locate<Folder>(path).newFile(newPath.fileName(), behavior);
    }

    verifyWriteAccess();

    if (behavior == ReplaceExisting && has(newPath))
    {
        try
        {
            removeFile(newPath);
        }
        catch (Feed::RemoveError const &er)
        {
            LOG_RES_WARNING("Failed to replace %s: existing file could not be removed.\n")
                    << newPath << er.asText();
        }
    }

    // The first feed able to create a file will get the honors.
    for (Feeds::iterator i = d->feeds.begin(); i != d->feeds.end(); ++i)
    {
        File *file = (*i)->newFile(newPath);
        if (file)
        {
            // Allow writing to the new file.
            file->setMode(Write);

            add(file);
            fileSystem().index(*file);
            return *file;
        }
    }

    /// @throw NewFileError All feeds of this folder failed to create a file.
    throw NewFileError("Folder::newFile", "Unable to create new file '" + newPath +
                       "' in " + description());
}

File &Folder::replaceFile(String const &newPath)
{
    return newFile(newPath, ReplaceExisting);
}

void Folder::removeFile(String const &removePath)
{
    DENG2_GUARD(this);

    String path = removePath.fileNamePath();
    if (!path.empty())
    {
        // Locate the folder where the file will be removed.
        return locate<Folder>(path).removeFile(removePath.fileName());
    }

    verifyWriteAccess();

    // It should now be in this folder.
    File *file = &locate<File>(removePath);
    Feed *originFeed = file->originFeed();

    // This'll close it and remove it from the index.
    delete file;

    // The origin feed will remove the original data of the file (e.g., the native file).
    if (originFeed)
    {
        originFeed->removeFile(removePath);
    }
}

bool Folder::has(String const &name) const
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

    DENG2_GUARD(d);
    return (d->contents.find(name.lower()) != d->contents.end());
}

File &Folder::add(File *file)
{
    DENG2_ASSERT(file != 0);

    if (has(file->name()))
    {
        /// @throw DuplicateNameError All file names in a folder must be unique.
        throw DuplicateNameError("Folder::add", "Folder cannot contain two files with the same name: '" +
            file->name() + "'");
    }
    DENG2_GUARD(d);
    d->add(file);
    return *file;
}

File *Folder::remove(String name)
{
    DENG2_GUARD(d);

    String const key = name.toLower();
    DENG2_ASSERT(d->contents.contains(key));

    File *removed = d->contents.take(key);
    removed->setParent(nullptr);
    return removed;
}

File *Folder::remove(char const *nameUtf8)
{
    return remove(String(nameUtf8));
}

File *Folder::remove(File &file)
{
    return remove(file.name());
}

filesys::Node const *Folder::tryGetChild(String const &name) const
{
    DENG2_GUARD(d);

    Contents::const_iterator found = d->contents.find(name.toLower());
    if (found != d->contents.end())
    {
        return found.value();
    }
    return nullptr;
}

void Folder::waitForPopulation()
{
    internal::populateTasks.waitForDone();
}

bool Folder::isPopulatingAsync()
{
    return !internal::populateTasks.isDone();
}

filesys::Node const *Folder::tryFollowPath(PathRef const &path) const
{
    // Absolute paths refer to the file system root.
    if (path.isAbsolute())
    {
        return fileSystem().root().tryFollowPath(path.subPath(Rangei(1, path.segmentCount())));
    }

    return Node::tryFollowPath(path);
}

File *Folder::tryLocateFile(String const &path) const
{
    if (filesys::Node const *node = tryFollowPath(Path(path)))
    {
        return const_cast<File *>(node->maybeAs<File>());
    }
    return nullptr;
}

void Folder::attach(Feed *feed)
{
    if (feed)
    {
        DENG2_GUARD(d);
        d->feeds.push_back(feed);
    }
}

Feed *Folder::detach(Feed &feed)
{
    DENG2_GUARD(d);

    d->feeds.removeOne(&feed);
    return &feed;
}

void Folder::setPrimaryFeed(Feed &feed)
{
    DENG2_GUARD(d);

    d->feeds.removeOne(&feed);
    d->feeds.push_front(&feed);
}

void Folder::clearFeeds()
{
    DENG2_GUARD(d);

    while (!d->feeds.empty())
    {
        delete detach(*d->feeds.front());
    }
}

Folder::Feeds Folder::feeds() const
{
    DENG2_GUARD(d);

    return d->feeds;
}

String Folder::contentsAsText() const
{
    QList<File const *> files;
    forContents([&files] (String, File &f) {
        files << &f;
        return LoopContinue;
    });
    return File::fileListAsText(files);
}

// Folder::Accessor ---------------------------------------------------------------------

Folder::Accessor::Accessor(Folder &owner, Property prop) : _owner(owner), _prop(prop)
{}

void Folder::Accessor::update() const
{
    DENG2_GUARD(_owner);

    // We need to alter the value content.
    Accessor *nonConst = const_cast<Accessor *>(this);

    switch (_prop)
    {
    case CONTENT_SIZE:
        nonConst->setValue(QString::number(_owner.d->contents.size()));
        break;
    }
}

Value *Folder::Accessor::duplicateContent() const
{
    return new NumberValue(asNumber());
}

} // namespace de
