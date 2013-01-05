/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "de/Folder"
#include "de/Feed"
#include "de/FS"
#include "de/NumberValue"
#include "de/Log"

using namespace de;

Folder::Folder(String const &name) : File(name)
{
    setStatus(Status::FOLDER);
    
    // Standard info.
    info().add(new Variable("contentSize", new Accessor(*this, Accessor::CONTENT_SIZE),
        Accessor::VARIABLE_MODE));
}

Folder::~Folder()
{
    DENG2_FOR_AUDIENCE(Deletion, i) i->fileBeingDeleted(*this);
    audienceForDeletion.clear();
    
    deindex();
    
    // Empty the contents.
    clear();
    
    // Destroy all feeds that remain.
    for(Feeds::reverse_iterator i = _feeds.rbegin(); i != _feeds.rend(); ++i)
    {
        delete *i;
    }
}

String Folder::describe() const
{
    String desc = String("folder \"%1\" (with %2 items from %3 feeds")
            .arg(name()).arg(_contents.size()).arg(_feeds.size());

    if(!_feeds.empty())
    {
        int n = 0;
        DENG2_FOR_EACH_CONST(Feeds, i, _feeds)
        {
            desc += String("; feed #%1 is %2").arg(++n).arg((*i)->description());
        }
    }
    return desc + ")";
}

void Folder::clear()
{
    if(_contents.empty()) return;
    
    // Destroy all the file objects.
    for(Contents::iterator i = _contents.begin(); i != _contents.end(); ++i)
    {
        i->second->setParent(0);
        delete i->second;
    }
    _contents.clear();
}

void Folder::populate(PopulationBehavior behavior)
{
    LOG_AS("Folder");
    
    // Prune the existing files first.
    for(Contents::iterator i = _contents.begin(); i != _contents.end(); )
    {
        // By default we will NOT prune if there are no feeds attached to the folder.
        // In this case the files were probably created manually, so we shouldn't
        // touch them.
        bool mustPrune = false;

        File *file = i->second;
        
        // If the file has a designated feed, ask it about pruning.
        if(file->originFeed() && file->originFeed()->prune(*file))
        {
            LOG_DEBUG("Pruning ") << file->path();
            mustPrune = true;
        }
        else if(!file->originFeed())
        {
            // There is no designated feed, ask all feeds of this folder.
            // If even one of the feeds thinks that the file is out of date,
            // it will be pruned.
            for(Feeds::iterator f = _feeds.begin(); f != _feeds.end(); ++f)
            {
                if((*f)->prune(*file))
                {
                    LOG_DEBUG("Pruning ") << file->path();
                    mustPrune = true;
                    break;
                }
            }
        }

        if(mustPrune)
        {
            // It needs to go.
            _contents.erase(i++);
            delete file;
        }
        else
        {
            ++i;
        }
    }
    
    // Populate with new/updated ones.
    for(Feeds::reverse_iterator i = _feeds.rbegin(); i != _feeds.rend(); ++i)
    {
        (*i)->populate(*this);
    }
    
    if(behavior == PopulateFullTree)
    {
        // Call populate on subfolders.
        for(Contents::iterator i = _contents.begin(); i != _contents.end(); ++i)
        {
            Folder *folder = dynamic_cast<Folder *>(i->second);
            if(folder)
            {
                folder->populate();
            }
        }
    }
}

Folder::Contents const &Folder::contents() const
{
    return _contents;
}

File &Folder::newFile(String const &newPath, bool replaceExisting)
{
    String path = newPath.fileNamePath();
    if(!path.empty())
    {
        // Locate the folder where the file will be created in.
        return locate<Folder>(path).newFile(newPath.fileName(), replaceExisting);
    }
    
    verifyWriteAccess();

    if(replaceExisting && has(newPath))
    {
        try
        {
            removeFile(newPath);
        }
        catch(Feed::RemoveError const &er)
        {
            LOG_WARNING("Failed to replace %s: existing file could not be removed.\n")
                    << newPath << er.asText();
        }
    }
    
    // The first feed able to create a file will get the honors.
    for(Feeds::iterator i = _feeds.begin(); i != _feeds.end(); ++i)
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
        "' in folder '" + Folder::path() + "'");
}

File &Folder::replaceFile(String const &newPath)
{
    return newFile(newPath, true);
}

void Folder::removeFile(String const &removePath)
{
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
    return (_contents.find(name.lower()) != _contents.end());
}

File &Folder::add(File *file)
{
    DENG2_ASSERT(file != 0);
    if(has(file->name()))
    {
        /// @throw DuplicateNameError All file names in a folder must be unique.
        throw DuplicateNameError("Folder::add", "Folder cannot contain two files with the same name: '" +
            file->name() + "'");
    }
    _contents[file->name().lower()] = file;
    file->setParent(this);
    return *file;
}

File *Folder::remove(File &file)
{
    for(Contents::iterator i = _contents.begin(); i != _contents.end(); ++i)
    {
        if(i->second == &file)
        {
            _contents.erase(i);
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
        return const_cast<Folder *>(this);
    }

    if(path[0] == '/')
    {
        // Route back to the root of the file system.
        return fileSystem().root().tryLocateFile(path.substr(1));
    }

    // Extract the next component.
    String::size_type end = path.indexOf('/');
    if(end == String::npos)
    {
        // No more slashes. What remains is the final component.
        Contents::const_iterator found = _contents.find(path.lower());
        if(found != _contents.end())
        {
            return found->second;
        }
        return 0;
    }

    String component = path.substr(0, end);
    String remainder = path.substr(end + 1);

    // Check for some special cases.
    if(component == ".")
    {
        return tryLocateFile(remainder);
    }
    if(component == "..")
    {
        if(!parent())
        {
            // Can't go there.
            return 0;
        }
        return parent()->tryLocateFile(remainder);
    }
    
    // Do we have a folder for this?
    Contents::const_iterator found = _contents.find(component.lower());
    if(found != _contents.end())
    {
        Folder *subFolder = dynamic_cast<Folder *>(found->second);
        if(subFolder)
        {
            // Continue recursively to the next component.
            return subFolder->tryLocateFile(remainder);
        }
    }
    
    // Dead end.
    return 0;
}

void Folder::attach(Feed *feed)
{
    _feeds.push_back(feed);
}

Feed *Folder::detach(Feed &feed)
{
    _feeds.remove(&feed);
    return &feed;
}

Folder::Accessor::Accessor(Folder &owner, Property prop) : _owner(owner), _prop(prop)
{}

void Folder::Accessor::update() const
{
    // We need to alter the value content.
    Accessor *nonConst = const_cast<Accessor *>(this);
    
    switch(_prop)
    {
    case CONTENT_SIZE:
        nonConst->setValue(QString::number(_owner._contents.size()));
        break;
    }    
}

Value *Folder::Accessor::duplicateContent() const
{
    return new NumberValue(asNumber());
}
