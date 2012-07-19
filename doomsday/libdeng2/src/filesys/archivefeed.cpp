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

#include "de/ArchiveFeed"
#include "de/ArchiveFile"
#include "de/Archive"
#include "de/Writer"
#include "de/Folder"
#include "de/FS"
#include "de/Log"

using namespace de;

ArchiveFeed::ArchiveFeed(File& archiveFile) : _file(archiveFile), _archive(0), _parentFeed(0)
{
    // Open the archive.
    _archive = new Archive(archiveFile);
}

ArchiveFeed::ArchiveFeed(ArchiveFeed& parentFeed, const String& basePath)
    : _file(parentFeed._file), _archive(0), _basePath(basePath), _parentFeed(&parentFeed)
{}

ArchiveFeed::~ArchiveFeed()
{
    LOG_AS("ArchiveFeed::~ArchiveFeed");
    
    if(_archive)
    {
        // If modified, the archive is written.
        if(_archive->modified())
        {
            LOG_MSG("Updating archive in ") << _file.name();

            // Make sure we have either a compressed or uncompressed version of
            // each entry in memory before destroying the source file.
            _archive->cache();

            _file.clear();
            Writer(_file) << *_archive;
        }        
        else
        {
            LOG_VERBOSE("Not updating archive in %s (not changed)") << _file.name();
        }
        delete _archive;
    }
}

void ArchiveFeed::populate(Folder& folder)
{
    LOG_AS("ArchiveFeed::populate");

    Archive::Names names;
    
    // Get a list of the files in this directory.
    archive().listFiles(names, _basePath);
    
    for(Archive::Names::iterator i = names.begin(); i != names.end(); ++i)
    {
        if(folder.has(*i))
        {
            // Already has an entry for this, skip it (wasn't pruned so it's OK).
            return;
        }
        
        String entry = _basePath / *i;
        
        std::auto_ptr<ArchiveFile> archFile(new ArchiveFile(*i, archive(), entry));
        // Use the status of the entry within the archive.
        archFile->setStatus(archive().status(entry));
        
        // Create a new file that accesses this feed's archive and interpret the contents.
        File* file = folder.fileSystem().interpret(archFile.release());
        folder.add(file);

        // We will decide on pruning this.
        file->setOriginFeed(this);
        
        // Include the file in the main index.
        folder.fileSystem().index(*file);
    }
    
    // Also populate subfolders.
    archive().listFolders(names, _basePath);
    
    for(Archive::Names::iterator i = names.begin(); i != names.end(); ++i)
    {
        String subBasePath = _basePath / *i;
        Folder& subFolder = folder.fileSystem().getFolder(folder.path() / *i);
        
        // Does it already have the appropriate feed?
        for(Folder::Feeds::const_iterator i = subFolder.feeds().begin();
            i != subFolder.feeds().end(); ++i)
        {
            ArchiveFeed* archFeed = const_cast<ArchiveFeed*>(dynamic_cast<const ArchiveFeed*>(*i));
            if(archFeed && &archFeed->archive() == &archive() && archFeed->basePath() == subBasePath)
            {
                // It's got it.
                LOG_DEBUG("Feed for ") << archFeed->basePath() << " already there.";
                return;
            }
        }
        
        // Create a new feed.
        subFolder.attach(new ArchiveFeed(*this, subBasePath));
    }
}

bool ArchiveFeed::prune(File& /*file*/) const
{
    /// @todo  Prune based on entry status.
    return true;
}

File* ArchiveFeed::newFile(const String& name)
{
    String newEntry = _basePath / name;
    if(archive().has(newEntry))
    {
        /// @throw AlreadyExistsError  The entry @a name already exists in the archive.
        throw AlreadyExistsError("ArchiveFeed::newFile", name + ": already exists");
    }
    // Add an empty entry.
    archive().add(newEntry, Block());
    File* file = new ArchiveFile(name, archive(), newEntry);
    file->setOriginFeed(this);
    return file;
}

void ArchiveFeed::removeFile(const String& name)
{
    String entryPath = _basePath / name;
    archive().remove(entryPath);
}

Archive& ArchiveFeed::archive()
{
    if(_parentFeed)
    {
        return _parentFeed->archive();
    }
    return *_archive;
}
