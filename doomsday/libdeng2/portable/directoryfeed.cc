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

#include "de/directoryfeed.h"
#include "de/folder.h"
#include "de/nativefile.h"
#include "de/string.h"
#include "de/fs.h"

#ifdef UNIX
#   include <sys/types.h>
#   include <dirent.h>
#   include <unistd.h>
#   include <string.h>
#   include <errno.h>
#endif

#ifdef WIN32
#   include <io.h>
#endif

using namespace de;

DirectoryFeed::DirectoryFeed(const std::string& nativePath)
    : nativePath_(nativePath)
{
    std::cout << "DirectoryFeed: " << nativePath << "\n";
}

DirectoryFeed::~DirectoryFeed()
{}

void DirectoryFeed::populate(Folder& folder)
{
#ifdef UNIX
    DIR* dir = opendir(nativePath_.empty()? "." : nativePath_.c_str());
    if(!dir)
    {
        throw NotFoundError("DirectoryFeed::populate", "Path '" + nativePath_ + "' not found");
    }
    struct dirent* entry;
    while((entry = readdir(dir)) != NULL)   
    {
        const std::string entryName = entry->d_name;
        switch(entry->d_type)
        {
        case DT_DIR:
            populateSubFolder(folder, entryName);
            break;
            
        default:
            populateFile(folder, entryName);
            break;
        }
    } 
    closedir(dir);
#endif

#ifdef WIN32
    _finddata_t fd;
    intptr_t handle;
    if((handle = _findfirst(nativePath_.concatenateNativePath("*").c_str(), &fd)) != -1L)
    {
        do
        {
            const std::string entryName = fd.name;
            if(fd.attrib & _A_SUBDIR)
            {
                populateSubFolder(folder, entryName);
            }
            else
            {
                populateFile(folder, entryName);
            }
        } 
        while(!_findnext(handle, &fd));
        _findclose(handle);
    }
#endif
}

void DirectoryFeed::populateSubFolder(Folder& folder, const std::string& entryName)
{
    if(entryName != "." && entryName != "..")
    {
        String subFeedPath = nativePath_.concatenateNativePath(entryName);
        Folder* subFolder = folder.locate<Folder>(entryName);
        if(!subFolder)
        {
            // This folder does not exist yet. Let's create it.
            subFolder = new Folder(entryName);
            folder.add(subFolder);
            folder.fileSystem().index(*subFolder);
        }
        else
        {        
            // The folder already exists. It may also already be fed by a DirectoryFeed.
            for(Folder::Feeds::const_iterator i = subFolder->feeds().begin();
                i != subFolder->feeds().end(); ++i)
            {
                const DirectoryFeed* dirFeed = dynamic_cast<const DirectoryFeed*>(*i);
                if(dirFeed && dirFeed->nativePath_ == subFeedPath)
                {
                    // Already got this fed. Nothing else needs done.
                    std::cout << "Feed for " << subFeedPath << " already there.\n";
                    return;
                }
            }
        }

        // Add a new feed.
        DirectoryFeed* feed = new DirectoryFeed(subFeedPath);
        subFolder->attach(feed);
    }
}

void DirectoryFeed::populateFile(Folder& folder, const std::string& entryName)
{
    if(folder.has(entryName))
    {
        // Already has an entry for this, skip it.
        return;
    }

    File* file = folder.fileSystem().interpret(new NativeFile(entryName, 
        nativePath_.concatenateNativePath(entryName)));

    // We will decide on pruning this.
    file->setOriginFeed(this);
    
    folder.add(file);
        
    // Include files the main index.
    folder.fileSystem().index(*file);
}

bool DirectoryFeed::prune(File& file) const
{
    /** 
     * @todo Rules for pruning:
     * - A NativeFile will be pruned if it's out of sync with the hard drive version (size, mod time).
     * - A Folder will be pruned if the corresponding directory does not exist.
     * - Other types of Files will not be pruned.
     */
    
    // Everything must go!
    return true;
}

void DirectoryFeed::changeWorkingDir(const std::string& nativePath)
{
#ifdef UNIX
    if(chdir(nativePath.c_str()))
    {
        throw WorkingDirError("DirectoryFeed::changeWorkingDir",
            nativePath + ": " + strerror(errno));
    }
#endif
}
