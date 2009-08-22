/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <sstream>

using namespace de;

Folder::Folder(const String& name) : File(name)
{
    setStatus(Status::FOLDER);
    
    // Standard info.
    info().add(new Variable("contentSize", new Accessor(*this, Accessor::CONTENT_SIZE),
        Accessor::VARIABLE_MODE));
}

Folder::~Folder()
{
    deindex();
    
    // Empty the contents.
    clear();
    
    // Destroy all feeds that remain.
    for(Feeds::reverse_iterator i = feeds_.rbegin(); i != feeds_.rend(); ++i)
    {
        delete *i;
    }
}

void Folder::clear()
{
    if(contents_.empty()) return;
    
    // Destroy all the file objects.
    for(Contents::iterator i = contents_.begin(); i != contents_.end(); ++i)
    {
        i->second->setParent(0);
        delete i->second;
    }
    contents_.clear();
}

void Folder::populate()
{
    LOG_AS("Folder::populate");
    
    // Prune the existing files first.
    for(Contents::iterator i = contents_.begin(); i != contents_.end(); )
    {
        // By default we will NOT prune if there are no feeds attached to the folder.
        // In this case the files were probably created manually, so we shouldn't
        // touch them.
        bool mustPrune = false;

        File* file = i->second;
        
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
            for(Feeds::iterator f = feeds_.begin(); f != feeds_.end(); ++f)
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
            contents_.erase(i++);
            delete file;
        }
        else
        {
            ++i;
        }
    }
    
    // Populate with new/updated ones.
    for(Feeds::reverse_iterator i = feeds_.rbegin(); i != feeds_.rend(); ++i)
    {
        (*i)->populate(*this);
    }
    
    // Call populate on subfolders.
    for(Contents::iterator i = contents_.begin(); i != contents_.end(); ++i)
    {
        Folder* folder = dynamic_cast<Folder*>(i->second);
        if(folder)
        {
            folder->populate();
        }
    }
}

const Folder::Contents& Folder::contents() const
{
    return contents_;
}

File& Folder::newFile(const String& newPath, bool replaceExisting)
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
        removeFile(newPath);
    }
    
    // The first feed able to create a file will get the honors.
    for(Feeds::iterator i = feeds_.begin(); i != feeds_.end(); ++i)
    {
        File* file = (*i)->newFile(newPath);
        if(file)
        {
            // Allow writing to the new file.
            file->setMode(WRITE);
            
            add(file);
            fileSystem().index(*file);
            return *file;
        }
    }

    /// @throw NewFileError All feeds of this folder failed to create a file.
    throw NewFileError("Folder::newFile", "Unable to create new file '" + newPath + 
        "' in folder '" + Folder::path() + "'");
}

File& Folder::replaceFile(const String& newPath)
{
    return newFile(newPath, true);
}

void Folder::removeFile(const String& removePath)
{
    String path = removePath.fileNamePath();
    if(!path.empty())
    {
        // Locate the folder where the file will be removed.
        return locate<Folder>(path).removeFile(removePath.fileName());
    }
    
    verifyWriteAccess();
    
    // It should now be in this folder.
    File& file = locate<File>(removePath);
    Feed* originFeed = file.originFeed();

    // This'll close it and remove it from the index.
    delete &file;
    
    // The origin feed will remove the original data of the file (e.g., the native file).
    if(originFeed)
    {
        originFeed->removeFile(removePath);
    }
    
}

bool Folder::has(const String& name) const
{
    return (contents_.find(name.lower()) != contents_.end());
}

File& Folder::add(File* file)
{
    assert(file != 0);
    if(has(file->name()))
    {
        /// @throw DuplicateNameError All file names in a folder must be unique.
        throw DuplicateNameError("Folder::add", "Folder cannot contain two files with the same name: '" +
            file->name() + "'");
    }
    contents_[file->name().lower()] = file;
    file->setParent(this);
    return *file;
}

File* Folder::remove(File& file)
{
    for(Contents::iterator i = contents_.begin(); i != contents_.end(); ++i)
    {
        if(i->second == &file)
        {
            contents_.erase(i);
            break;
        }
    }    
    file.setParent(0);
    return &file;
}

File* Folder::tryLocateFile(const String& path) const
{
    if(path.empty())
    {
        return const_cast<Folder*>(this);
    }

    if(path[0] == '/')
    {
        // Route back to the root of the file system.
        return fileSystem().root().tryLocateFile(path.substr(1));
    }

    // Extract the next component.
    String::size_type end = path.find('/');
    if(end == String::npos)
    {
        // No more slashes. What remains is the final component.
        Contents::const_iterator found = contents_.find(path.lower());
        if(found != contents_.end())
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
    Contents::const_iterator found = contents_.find(component.lower());
    if(found != contents_.end())
    {
        Folder* subFolder = dynamic_cast<Folder*>(found->second);
        if(subFolder)
        {
            // Continue recursively to the next component.
            return subFolder->tryLocateFile(remainder);
        }
    }
    
    // Dead end.
    return 0;
}

void Folder::attach(Feed* feed)
{
    feeds_.push_back(feed);
}

Feed* Folder::detach(Feed& feed)
{
    feeds_.remove(&feed);
    return &feed;
}

Folder::Accessor::Accessor(Folder& owner, Property prop) : owner_(owner), prop_(prop)
{}

void Folder::Accessor::update() const
{
    // We need to alter the value content.
    Accessor* nonConst = const_cast<Accessor*>(this);
    
    std::ostringstream os;
    switch(prop_)
    {
    case CONTENT_SIZE:
        os << owner_.contents_.size();
        nonConst->setValue(os.str());
        break;
    }    
}

Value* Folder::Accessor::duplicateContent() const
{
    return new NumberValue(asNumber());
}
