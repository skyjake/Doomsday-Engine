/**
 * @file fs.cpp File System
 *
 * @author Copyright &copy; 2009-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2012 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "de/FS"
#include "de/LibraryFile"
#include "de/DirectoryFeed"
#include "de/ArchiveFeed"
#include "de/NativePath"
#include "de/PackageFolder"
#include "de/ZipArchive"
#include "de/Log"

using namespace de;

static FS::Index const emptyIndex;

struct FS::Instance
{
    /// The main index to all files in the file system.
    FS::Index index;

    /// Index of file types. Each entry in the index is another index of names
    /// to file instances.
    typedef std::map<String, Index> TypeIndex;
    TypeIndex typeIndex;

    /// The root folder of the entire file system.
    Folder root;
};

FS::FS()
{
    d = new Instance;
}

FS::~FS()
{
    delete d;
}

void FS::refresh()
{
    LOG_AS("FS::refresh");

    Time startedAt;
    d->root.populate();

    LOG_DEBUG("Done in %.2f seconds.") << startedAt.since();
    
    printIndex();
}

Folder &FS::makeFolder(String const &path)
{
    Folder *subFolder = d->root.tryLocate<Folder>(path);
    if(!subFolder)
    {
        // This folder does not exist yet. Let's create it.
        Folder &parentFolder = makeFolder(path.fileNamePath());
        subFolder = new Folder(path.fileName());
        parentFolder.add(subFolder);
        index(*subFolder);
    }
    return *subFolder;
}

File *FS::interpret(File *sourceData)
{
    LOG_AS("FS::interpret");
    
    /// @todo  One should be able to define new interpreters dynamically.
    
    try
    {
        if(LibraryFile::recognize(*sourceData))
        {
            LOG_VERBOSE("Interpreted ") << sourceData->description() << " as a shared library";
        
            // It is a shared library intended for Doomsday.
            return new LibraryFile(sourceData);
        }
        if(ZipArchive::recognize(*sourceData))
        {
            try
            {
                LOG_VERBOSE("Interpreted %s as a ZIP format archive") << sourceData->description();

                // It is a ZIP archive: we will represent it as a folder.
                std::auto_ptr<PackageFolder> package(new PackageFolder(*sourceData, sourceData->name()));

                // Archive opened successfully, give ownership of the source to the folder.
                package->setSource(sourceData);
                return package.release();
            }
            catch(Archive::FormatError const &)
            {
                // Even though it was recognized as an archive, the file
                // contents may still prove to be corrupted.
                LOG_WARNING("Archive in %s is invalid") << sourceData->description();
            }
            catch(IByteArray::OffsetError const &)
            {
                LOG_WARNING("Archive in %s is truncated") << sourceData->description();
            }
            catch(IIStream::InputError const &)
            {
                LOG_WARNING("%s cannot be read") << sourceData->description();
            }
        }
    }
    catch(Error const &err)
    {
        LOG_ERROR("") << err.asText();

        // The error is one we don't know how to handle. We were given
        // responsibility of the source file, so it has to be deleted.
        delete sourceData;

        throw;
    }
    return sourceData;
}

FS::Index const &FS::nameIndex() const
{
    return d->index;
}

int FS::findAll(String const &path, FoundFiles &found) const
{
    LOG_AS("FS::findAll");

    found.clear();
    String baseName = path.fileName().lower();
    String dir = path.fileNamePath().lower();
    if(!dir.empty() && !dir.beginsWith("/"))
    {
        // Always begin with a slash. We don't want to match partial folder names.
        dir = "/" + dir;
    }

    ConstIndexRange range = d->index.equal_range(baseName);
    for(Index::const_iterator i = range.first; i != range.second; ++i)    
    {       
        File *file = i->second;
        if(file->path().endsWith(dir))
        {
            found.push_back(file);
        }
    }
    return found.size();
}

File &FS::find(String const &path) const
{
    return find<File>(path);
}

void FS::index(File &file)
{
    String const lowercaseName = file.name().lower();
    
    d->index.insert(IndexEntry(lowercaseName, &file));
    
    // Also make an entry in the type index.
    Index &indexOfType = d->typeIndex[DENG2_TYPE_NAME(file)];
    indexOfType.insert(IndexEntry(lowercaseName, &file));
}

static void removeFromIndex(FS::Index &idx, File &file)
{
    if(idx.empty()) 
    {
        return;
    }

    // Look up the ones that might be this file.
    FS::IndexRange range = idx.equal_range(file.name().lower());

    for(FS::Index::iterator i = range.first; i != range.second; ++i)
    {
        if(i->second == &file)
        {
            // This is the one to deindex.
            idx.erase(i);
            break;
        }
    }
}

void FS::deindex(File &file)
{
    removeFromIndex(d->index, file);
    removeFromIndex(d->typeIndex[DENG2_TYPE_NAME(file)], file);
}

FS::Index const &FS::indexFor(String const &typeName) const
{
    Instance::TypeIndex::const_iterator found = d->typeIndex.find(typeName);
    if(found != d->typeIndex.end())
    {
        return found->second;
    }
    return emptyIndex;
    /*
    /// @throw UnknownTypeError No files of type @a typeName have been indexed.
    throw UnknownTypeError("FS::indexFor", "No files of type '" + typeName + "' have been indexed");
    */
}

void FS::printIndex()
{
    LOG_DEBUG("Main FS index has %i entries") << d->index.size();

    for(Index::iterator i = d->index.begin(); i != d->index.end(); ++i)
    {
        LOG_TRACE("\"%s\": ") << i->first << i->second->description();
    }
    
    for(Instance::TypeIndex::iterator k = d->typeIndex.begin(); k != d->typeIndex.end(); ++k)
    {
        LOG_DEBUG("Index for type '%s' has %i entries") << k->first << k->second.size();

        LOG_AS_STRING(k->first);
        for(Index::iterator i = k->second.begin(); i != k->second.end(); ++i)
        {
            LOG_TRACE("\"%s\": ") << i->first << i->second->description();
        }
    }
}

Folder &FS::root()
{
    return d->root;
}
