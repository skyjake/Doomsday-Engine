/*
 * The Doomsday Engine Project -- libdeng2
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

namespace de {

DENG2_PIMPL_NOREF(Folder)
{
    /// A map of file names to file instances.
    Contents contents;

    /// Feeds provide content for the folder.
    Feeds feeds;
};

Folder::Folder(String const &name) : File(name), d(new Instance)
{
    setStatus(Status::FOLDER);
    
    // Standard info.
    info().add(new Variable("contentSize",
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
    for(Feeds::reverse_iterator i = d->feeds.rbegin(); i != d->feeds.rend(); ++i)
    {
        delete *i;
    }
}

String Folder::describe() const
{
    DENG2_GUARD(this);

    String desc = String("folder \"%1\"").arg(name());

    String const feedDesc = describeFeeds();
    if(!feedDesc.isEmpty())
    {
        desc += String(" (%1)").arg(feedDesc);
    }

    return desc;
}

String Folder::describeFeeds() const
{
    DENG2_GUARD(this);

    String desc;

    if(d->feeds.size() == 1)
    {
        desc += String("contains %1 file%2 from %3")
                .arg(d->contents.size())
                .arg(DENG2_PLURAL_S(d->contents.size()))
                .arg(d->feeds.front()->description());
    }
    else if(d->feeds.size() > 1)
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
    DENG2_GUARD(this);

    if(d->contents.empty()) return;
    
    // Destroy all the file objects.
    for(Contents::iterator i = d->contents.begin(); i != d->contents.end(); ++i)
    {
        i->second->setParent(0);
        delete i->second;
    }
    d->contents.clear();
}

void Folder::populate(PopulationBehavior behavior)
{
    DENG2_GUARD(this);

    LOG_AS("Folder");
    
    // Prune the existing files first.
    for(Contents::iterator i = d->contents.begin(); i != d->contents.end(); )
    {
        // By default we will NOT prune if there are no feeds attached to the folder.
        // In this case the files were probably created manually, so we shouldn't
        // touch them.
        bool mustPrune = false;

        File *file = i->second;
        
        // If the file has a designated feed, ask it about pruning.
        if(file->originFeed() && file->originFeed()->prune(*file))
        {
            LOG_RES_XVERBOSE("Pruning \"%s\"") << file->path();
            mustPrune = true;
        }
        else if(!file->originFeed())
        {
            // There is no designated feed, ask all feeds of this folder.
            // If even one of the feeds thinks that the file is out of date,
            // it will be pruned.
            for(Feeds::iterator f = d->feeds.begin(); f != d->feeds.end(); ++f)
            {
                if((*f)->prune(*file))
                {
                    LOG_RES_XVERBOSE("Pruning ") << file->path();
                    mustPrune = true;
                    break;
                }
            }
        }

        if(mustPrune)
        {
            // It needs to go.
            d->contents.erase(i++);
            delete file;
        }
        else
        {
            ++i;
        }
    }
    
    // Populate with new/updated ones.
    for(Feeds::reverse_iterator i = d->feeds.rbegin(); i != d->feeds.rend(); ++i)
    {
        (*i)->populate(*this);
    }
    
    if(behavior == PopulateFullTree)
    {
        // Call populate on subfolders.
        for(Contents::iterator i = d->contents.begin(); i != d->contents.end(); ++i)
        {
            if(Folder *folder = i->second->maybeAs<Folder>())
            {
                folder->populate();
            }
        }
    }
}

Folder::Contents const &Folder::contents() const
{
    DENG2_GUARD(this);

    return d->contents;
}

File &Folder::newFile(String const &newPath, FileCreationBehavior behavior)
{
    DENG2_GUARD(this);

    String path = newPath.fileNamePath();
    if(!path.empty())
    {
        // Locate the folder where the file will be created in.
        return locate<Folder>(path).newFile(newPath.fileName(), behavior);
    }
    
    verifyWriteAccess();

    if(behavior == ReplaceExisting && has(newPath))
    {
        try
        {
            removeFile(newPath);
        }
        catch(Feed::RemoveError const &er)
        {
            LOG_RES_WARNING("Failed to replace %s: existing file could not be removed.\n")
                    << newPath << er.asText();
        }
    }
    
    // The first feed able to create a file will get the honors.
    for(Feeds::iterator i = d->feeds.begin(); i != d->feeds.end(); ++i)
    {
        File *file = (*i)->newFile(newPath);
        if(file)
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
    if(!path.empty())
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
    if(originFeed)
    {
        originFeed->removeFile(removePath);
    }    
}

bool Folder::has(String const &name) const
{
    DENG2_GUARD(this);

    if(name.isEmpty()) return false;

    // Check if we were given a path rather than just a name.
    String path = name.fileNamePath();
    if(!path.empty())
    {
        Folder *folder = tryLocate<Folder>(path);
        if(folder)
        {
            return folder->has(name.fileName());
        }
        return false;
    }

    return (d->contents.find(name.lower()) != d->contents.end());
}

File &Folder::add(File *file)
{
    DENG2_GUARD(this);

    DENG2_ASSERT(file != 0);

    if(has(file->name()))
    {
        /// @throw DuplicateNameError All file names in a folder must be unique.
        throw DuplicateNameError("Folder::add", "Folder cannot contain two files with the same name: '" +
            file->name() + "'");
    }
    d->contents[file->name().lower()] = file;
    file->setParent(this);
    return *file;
}

File *Folder::remove(String const &name)
{
    return remove(locate<File>(name));
}

File *Folder::remove(char const *nameUtf8)
{
    return remove(String(nameUtf8));
}

File *Folder::remove(File &file)
{
    DENG2_GUARD(this);

    for(Contents::iterator i = d->contents.begin(); i != d->contents.end(); ++i)
    {
        if(i->second == &file)
        {
            d->contents.erase(i);
            break;
        }
    }    
    file.setParent(0);
    return &file;
}

File *Folder::tryLocateFile(String const &path) const
{
    if(path.empty())
    {
        File *file = const_cast<Folder *>(this);
        return file;
    }

    if(path[0] == '/')
    {
        // Route back to the root of the file system.
        File *file = fileSystem().root().tryLocateFile(path.substr(1));
        return file;
    }

    DENG2_GUARD(this);

    // Extract the next component.
    String::size_type end = path.indexOf('/');
    if(end == String::npos)
    {
        // No more slashes. What remains is the final component.
        Contents::const_iterator found = d->contents.find(path.lower());
        if(found != d->contents.end())
        {
            File *file = found->second;
            return file;
        }
        return 0;
    }

    String component = path.substr(0, end);
    String remainder = path.substr(end + 1);

    // Check for some special cases.
    if(component == ".")
    {
        File *file = tryLocateFile(remainder);
        return file;
    }
    if(component == "..")
    {
        if(!parent())
        {
            // Can't go there.
            return 0;
        }
        File *file = parent()->tryLocateFile(remainder);
        return file;
    }
    
    // Do we have a folder for this?
    Contents::const_iterator found = d->contents.find(component.lower());
    if(found != d->contents.end())
    {
        if(Folder *subFolder = found->second->maybeAs<Folder>())
        {
            // Continue recursively to the next component.
            File *file = subFolder->tryLocateFile(remainder);
            return file;
        }
    }
    
    // Dead end.
    return 0;
}

void Folder::attach(Feed *feed)
{
    if(feed)
    {
        DENG2_GUARD(this);
        d->feeds.push_back(feed);
    }
}

Feed *Folder::detach(Feed &feed)
{
    DENG2_GUARD(this);

    d->feeds.remove(&feed);
    return &feed;
}

void Folder::setPrimaryFeed(Feed &feed)
{
    DENG2_GUARD(this);

    d->feeds.remove(&feed);
    d->feeds.push_front(&feed);
}

void Folder::clearFeeds()
{
    while(!d->feeds.empty())
    {
        delete detach(*d->feeds.front());
    }
}

Folder::Feeds const &Folder::feeds() const
{
    return d->feeds;
}

String Folder::contentsAsText() const
{
    QList<File const *> files;
    DENG2_FOR_EACH_CONST(Contents, i, contents())
    {
        files << i->second;
    }
    return File::fileListAsText(files);
}

Folder::Accessor::Accessor(Folder &owner, Property prop) : _owner(owner), _prop(prop)
{}

void Folder::Accessor::update() const
{
    DENG2_GUARD(_owner);

    // We need to alter the value content.
    Accessor *nonConst = const_cast<Accessor *>(this);
    
    switch(_prop)
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
