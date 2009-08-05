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

#include "de/ArchiveFeed"
#include "de/ArchiveFile"
#include "de/Archive"
#include "de/Writer"
#include "de/Folder"
#include "de/FS"

using namespace de;

ArchiveFeed::ArchiveFeed(File& archiveFile) : file_(archiveFile), archive_(0), parentFeed_(0)
{
    // Open the archive.
    archive_ = new Archive(archiveFile);
}

ArchiveFeed::ArchiveFeed(ArchiveFeed& parentFeed, const String& basePath)
    : file_(parentFeed.file_), archive_(0), basePath_(basePath), parentFeed_(&parentFeed)
{}

ArchiveFeed::~ArchiveFeed()
{
    if(archive_)
    {
        // If modified, the archive is written.
        if(archive_->modified())
        {
            std::cout << "Updating archive in " << file_.name() << "\n";

            // Make sure we have either a compressed or uncompressed version of
            // each entry in memory before destroying the source file.
            archive_->cache();

            file_.clear();
            Writer(file_) << *archive_;
        }        
        else
        {
            std::cout << "Not updating archive in " << file_.name() << " (not changed)\n";
        }
        delete archive_;
    }
}

void ArchiveFeed::populate(Folder& folder)
{
    Archive::Names names;
    
    // Get a list of the files in this directory.
    archive().listFiles(names, basePath_);
    
    for(Archive::Names::iterator i = names.begin(); i != names.end(); ++i)
    {
        if(folder.has(*i))
        {
            // Already has an entry for this, skip it (wasn't pruned so it's OK).
            return;
        }
        
        String entry = basePath_ / *i;
        
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
    archive().listFolders(names, basePath_);
    
    for(Archive::Names::iterator i = names.begin(); i != names.end(); ++i)
    {
        String subBasePath = basePath_ / *i;
        Folder& subFolder = folder.fileSystem().getFolder(folder.path() / *i);
        
        // Does it already have the appropriate feed?
        for(Folder::Feeds::const_iterator i = subFolder.feeds().begin();
            i != subFolder.feeds().end(); ++i)
        {
            ArchiveFeed* archFeed = const_cast<ArchiveFeed*>(dynamic_cast<const ArchiveFeed*>(*i));
            if(archFeed && &archFeed->archive() == &archive() && archFeed->basePath() == subBasePath)
            {
                // It's got it.
                std::cout << "Feed for " << archFeed->basePath() << " already there.\n";
                return;
            }
        }
        
        // Create a new feed.
        subFolder.attach(new ArchiveFeed(*this, subBasePath));
    }
}

bool ArchiveFeed::prune(File& file) const
{
    /// @todo  Prune based on entry status.
    return true;
}

File* ArchiveFeed::newFile(const String& name)
{
    String newEntry = basePath_ / name;
    if(archive().has(newEntry))
    {
        /// @throw AlreadyExistsError  The entry @a name already exists in the archive.
        throw AlreadyExistsError("ArchiveFeed::newFile", name + ": already exists");
    }
    // Add an empty entry.
    archive().add(newEntry, String());
    File* file = new ArchiveFile(name, archive(), newEntry);
    file->setOriginFeed(this);
    return file;
}

void ArchiveFeed::removeFile(const String& name)
{
    String entryPath = basePath_ / name;
    archive().remove(entryPath);
}

Archive& ArchiveFeed::archive()
{
    if(parentFeed_)
    {
        return parentFeed_->archive();
    }
    return *archive_;
}
