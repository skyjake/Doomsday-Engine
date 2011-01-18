/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009, 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/DirectoryFeed"
#include "de/Folder"
#include "de/NativeFile"
#include "de/String"
#include "de/FS"
#include "de/Date"
#include "de/Log"

#include <QDir>
#include <QFileInfo>

using namespace de;

DirectoryFeed::DirectoryFeed(const String& nativePath, const Flags& mode)
    : _nativePath(nativePath), _mode(mode) {}

DirectoryFeed::~DirectoryFeed()
{}

void DirectoryFeed::populate(Folder& folder)
{
    if(_mode & AllowWrite)
    {
        folder.setMode(File::Write);
    }
    if(_mode.testFlag(CreateIfMissing) && !exists(_nativePath))
    {
        createDir(_nativePath);
    }

    QDir dir(_nativePath);
    if(!dir.isReadable())
    {
        /// @throw NotFoundError The native directory was not accessible.
        throw NotFoundError("DirectoryFeed::populate", "Path '" + _nativePath + "' inaccessible");
    }
    QStringList nameFilters;
    nameFilters << "*";
    foreach(QFileInfo entry,
            dir.entryInfoList(nameFilters, QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot))
    {
        if(entry.isDir())
        {
            populateSubFolder(folder, entry.fileName());
        }
        else
        {
            populateFile(folder, entry.fileName());
        }
    }
}

void DirectoryFeed::populateSubFolder(Folder& folder, const String& entryName)
{
    LOG_AS("DirectoryFeed::populateSubFolder");

    if(entryName != "." && entryName != "..")
    {
        String subFeedPath = _nativePath.concatenateNativePath(entryName);
        Folder& subFolder = folder.fileSystem().getFolder(folder.path() / entryName);

        if(_mode & AllowWrite)
        {
            subFolder.setMode(File::Write);
        }

        // It may already be fed by a DirectoryFeed.
        for(Folder::Feeds::const_iterator i = subFolder.feeds().begin();
            i != subFolder.feeds().end(); ++i)
        {
            const DirectoryFeed* dirFeed = dynamic_cast<const DirectoryFeed*>(*i);
            if(dirFeed && dirFeed->_nativePath == subFeedPath)
            {
                // Already got this fed. Nothing else needs done.
                LOG_DEBUG("Feed for ") << subFeedPath << " already there.";
                return;
            }
        }

        // Add a new feed. Mode inherited.
        subFolder.attach(new DirectoryFeed(subFeedPath, _mode));
    }
}

void DirectoryFeed::populateFile(Folder& folder, const String& entryName)
{
    if(folder.has(entryName))
    {
        // Already has an entry for this, skip it (wasn't pruned so it's OK).
        return;
    }
    
    String entryPath = _nativePath.concatenateNativePath(entryName);

    // Open the native file.
    std::auto_ptr<NativeFile> nativeFile(new NativeFile(entryName, entryPath));
    nativeFile->setStatus(fileStatus(entryPath));
    if(_mode & AllowWrite)
    {
        nativeFile->setMode(File::Write);
    }

    File* file = folder.fileSystem().interpret(nativeFile.release());
    folder.add(file);

    // We will decide on pruning this.
    file->setOriginFeed(this);
        
    // Include files the main index.
    folder.fileSystem().index(*file);
}

bool DirectoryFeed::prune(File& file) const
{
    LOG_AS("DirectoryFeed::prune");
    
    /// Rules for pruning:
    /// - A file sourced by NativeFile will be pruned if it's out of sync with the hard 
    ///   drive version (size, time of last modification).
    NativeFile* nativeFile = dynamic_cast<NativeFile*>(file.source());
    if(nativeFile)
    {
        try
        {
            if(fileStatus(nativeFile->nativePath()) != nativeFile->status())
            {
                // It's not up to date.
                LOG_VERBOSE("%s: status has changed, pruning!") << nativeFile->nativePath();
                return true;
            }
        }
        catch(const StatusError&)
        {
            // Get rid of it.
            return true;
        }
    }
    
    /// - A Folder will be pruned if the corresponding directory does not exist (providing
    ///   a DirectoryFeed is the sole feed in the folder).
    Folder* subFolder = dynamic_cast<Folder*>(&file);
    if(subFolder)
    {
        if(subFolder->feeds().size() == 1)
        {
            DirectoryFeed* dirFeed = dynamic_cast<DirectoryFeed*>(subFolder->feeds().front());
            if(dirFeed && !exists(dirFeed->_nativePath))
            {
                LOG_VERBOSE("%s: no longer exists, pruning!") << _nativePath;
                return true;
            }
        }
    }

    /// - Other types of Files will not be pruned.
    return false;
}

File* DirectoryFeed::newFile(const String& name)
{
    String newPath = _nativePath.concatenateNativePath(name);
    if(exists(newPath))
    {
        /// @throw AlreadyExistsError  The file @a name already exists in the native directory.
        throw AlreadyExistsError("DirectoryFeed::newFile", name + ": already exists");
    }
    File* file = new NativeFile(name, newPath);
    file->setOriginFeed(this);
    return file;
}

void DirectoryFeed::removeFile(const String& name)
{
    String path = _nativePath.concatenateNativePath(name);
    if(!exists(path))
    {
        /// @throw NotFoundError  The file @a name does not exist in the native directory.
        throw NotFoundError("DirectoryFeed::removeFile", name + ": not found");
    }

    QDir::current().remove(path);
}

void DirectoryFeed::changeWorkingDir(const String& nativePath)
{
    if(!QDir::setCurrent(nativePath))
    {
        /// @throw WorkingDirError Changing to @a nativePath failed.
        throw WorkingDirError("DirectoryFeed::changeWorkingDir",
                              "Failed to change to " + nativePath);
    }
}

void DirectoryFeed::createDir(const String& nativePath)
{
    String parentPath = nativePath.fileNameNativePath();
    if(!parentPath.empty() && !exists(parentPath))
    {
        createDir(parentPath);
    }

    if(!QDir::current().mkdir(nativePath))
    {
        /// @throw CreateDirError Failed to create directory @a nativePath.
        throw CreateDirError("DirectoryFeed::createDir", "Could not create: " + nativePath);
    }
}

bool DirectoryFeed::exists(const String& nativePath)
{
    return QDir::current().exists(nativePath);
}

File::Status DirectoryFeed::fileStatus(const String& nativePath)
{
    QFileInfo info(nativePath);

    if(!info.exists())
    {
        /// @throw StatusError Determining the file status was not possible.
        throw StatusError("DirectoryFeed::fileStatus", nativePath + " inaccessible");
    }

    // Get file status information.
    return File::Status(info.size(), info.lastModified());
}
