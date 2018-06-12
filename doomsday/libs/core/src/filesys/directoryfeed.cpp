/** @file directoryfeed.cpp Directory Feed.
 *
 * @author Copyright © 2009-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright © 2013 Daniel Swanson <danij@dengine.net>
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

#include "de/DirectoryFeed"
#include "de/Folder"
#include "de/NativeFile"
#include "de/NativePath"
#include "de/FS"
#include "de/Date"
#include "de/App"

#include <fstream>
#include <c_plus/object.h>
#include <c_plus/fileinfo.h>

namespace de {

static String const fileStatusSuffix = ".doomsday_file_status";

DENG2_PIMPL_NOREF(DirectoryFeed)
{
    NativePath nativePath;
    Flags mode;
    String namePattern;
};

DirectoryFeed::DirectoryFeed(NativePath const &nativePath, Flags const &mode)
    : d(new Impl)
{
    d->nativePath = nativePath;
    d->mode = mode;
}

void DirectoryFeed::setNamePattern(const String &namePattern)
{
    d->namePattern = namePattern;
}

String DirectoryFeed::description() const
{
    String desc;
    if (d->namePattern)
    {
        desc = "files matching \"" + d->namePattern + "\" in ";
    }
    desc += "directory \"" + d->nativePath.pretty() + "\"";
    return desc;
}

const NativePath &DirectoryFeed::nativePath() const
{
    return d->nativePath;
}

Feed::PopulatedFiles DirectoryFeed::populate(Folder const &folder)
{
    if (d->mode & AllowWrite)
    {
        // Automatically enable modifying the Folder.
        const_cast<Folder &>(folder).setMode(File::Write);
    }
    if (d->mode.testFlag(CreateIfMissing) && !NativePath::exists(d->nativePath))
    {
        NativePath::createPath(d->nativePath);
    }

    if (!_nativePath.exists())
    {
        /// @throw NotFoundError The native directory was not accessible.
        throw NotFoundError("DirectoryFeed::populate", "Path '" + d->nativePath + "' inaccessible");
    }
//    QStringList nameFilters;
//    nameFilters << "*";
//    QDir::Filters dirFlags = QDir::Files | QDir::NoDotAndDotDot;
//    if (_mode.testFlag(PopulateNativeSubfolders))
//    {
//        dirFlags |= QDir::Dirs;
//    }

    PopulatedFiles populated;

    cplus::ref<iDirFileInfo> dirInfo(new_DirFileInfo(_nativePath.toString()));
    iForEach(DirFileInfo, i, dirInfo)
    {
        const String path = path_FileInfo(i.value);
        const CString name = path.fileName();

        // Filter out some files.
        if (!_mode.testFlag(PopulateNativeSubfolders) && isDirectory_FileInfo(i.value))
    {
            continue;
    }

        if (isDirectory_FileInfo(i.value))
        {
            populateSubFolder(folder, name);
        }
        else
        {
            if (!name.endsWith(fileStatusSuffix)) // ignore meta files
            {
                populateFile(folder, name, populated);
            }
        }
    }
    return populated;
}

void DirectoryFeed::populateSubFolder(Folder const &folder, String const &entryName)
{
    LOG_AS("DirectoryFeed::populateSubFolder");

    if (entryName != "." && entryName != "..")
    {
        Folder *subFolder = nullptr;
        if (!folder.has(entryName))
        {
            subFolder = &folder.fileSystem()
                    .makeFolderWithFeed(folder.path() / entryName,
                                        newSubFeed(entryName),
                                        Folder::PopulateFullTree,
                                        FS::DontInheritFeeds);
        }
        else
        {
            // Use the previously populated subfolder.
            subFolder = &folder.locate<Folder>(entryName);
        }

        if (d->mode & AllowWrite)
        {
            subFolder->setMode(File::Write);
        }
        else
        {
            subFolder->setMode(File::ReadOnly);
        }
    }
}

void DirectoryFeed::populateFile(Folder const &folder, String const &entryName,
                                 PopulatedFiles &populated)
{
    try
    {
        if (folder.has(entryName))
        {
            // Already has an entry for this, skip it (wasn't pruned so it's OK).
            return;
        }

        NativePath const entryPath = d->nativePath / entryName;

        // Open the native file.
        std::unique_ptr<NativeFile> nativeFile(new NativeFile(entryName, entryPath));
        nativeFile->setStatus(fileStatus(entryPath));
        if (d->mode & AllowWrite)
        {
            nativeFile->setMode(File::Write);
        }

        File *file = folder.fileSystem().interpret(nativeFile.release());

        // We will decide on pruning this.
        file->setOriginFeed(this);

        populated << file;
    }
    catch (StatusError const &er)
    {
        LOG_WARNING("Error with \"%s\" in %s: %s")
                << entryName
                << folder.description()
                << er.asText();
    }
}

bool DirectoryFeed::prune(File &file) const
{
    LOG_AS("DirectoryFeed::prune");

    /// Rules for pruning:
    /// - A file sourced by NativeFile will be pruned if it's out of sync with the hard
    ///   drive version (size, time of last modification).
    if (NativeFile *nativeFile = maybeAs<NativeFile>(file.source()))
    {
        try
        {
            if (fileStatus(nativeFile->nativePath()) != nativeFile->status())
            {
                // It's not up to date.
                LOG_RES_MSG("Pruning \"%s\": status has changed") << nativeFile->nativePath();
                return true;
            }
        }
        catch (StatusError const &)
        {
            // Get rid of it.
            return true;
        }
    }

    /// - A Folder will be pruned if the corresponding directory does not exist (providing
    ///   a DirectoryFeed is the sole feed in the folder).
    if (Folder *subFolder = maybeAs<Folder>(file))
    {
        if (subFolder->feeds().size() == 1)
        {
            DirectoryFeed *dirFeed = maybeAs<DirectoryFeed>(subFolder->feeds().front());
            if (dirFeed && !dirFeed->d->nativePath.exists())
            {
                LOG_RES_NOTE("Pruning %s: no longer exists") << dirFeed->description(); //d->nativePath;
                return true;
            }
        }
    }

    /// - Other types of Files will not be pruned.
    return false;
}

File *DirectoryFeed::createFile(String const &name)
{
    NativePath newPath = d->nativePath / name;
    /*if (NativePath::exists(newPath))
    {
        /// @throw AlreadyExistsError  The file @a name already exists in the native directory.
        //throw AlreadyExistsError("DirectoryFeed::createFile", name + ": already exists");
        //qDebug() << "[DirectoryFeed] Overwriting" << newPath.toString();
    }*/
    File *file = new NativeFile(name, newPath);
    file->setOriginFeed(this);
    return file;
}

void DirectoryFeed::destroyFile(String const &name)
{
    NativePath path = d->nativePath / name;

    if (!path.exists())
    {
        // The file doesn't exist in the native file system, we can ignore this.
        return;
    }
    if (!path.destroy())
    if (remove(path))
    {
        /// @throw RemoveError  The file @a name exists but could not be removed.
        throw RemoveError("DirectoryFeed::destroyFile", "Cannot remove \"" + name +
                          "\" in " + description() + ": " + strerror(errno));
    }
}

Feed *DirectoryFeed::newSubFeed(String const &name)
{
    NativePath subPath = d->nativePath / name;
    if (d->mode.testFlag(CreateIfMissing) || (subPath.exists() && subPath.isReadable()))
    {
        return new DirectoryFeed(subPath, d->mode);
    }
    return nullptr;
}

void DirectoryFeed::changeWorkingDir(NativePath const &nativePath)
{
    if (!App::setCurrentWorkPath(nativePath))
    {
        /// @throw WorkingDirError Changing to @a nativePath failed.
        throw WorkingDirError("DirectoryFeed::changeWorkingDir",
                              "Failed to change to " + nativePath);
    }
}

File::Status DirectoryFeed::fileStatus(NativePath const &nativePath)
{
    cplus::ref<iFileInfo> info(new_FileInfo(nativePath.toString()));
    if (!exists_FileInfo(info))
    {
        /// @throw StatusError Determining the file status was not possible.
        throw StatusError("DirectoryFeed::fileStatus", nativePath.toString() + " inaccessible");
    }

    // Get file status information.
    File::Status st { isDirectory_FileInfo(info)? File::Type::Folder : File::Type::File,
                      dsize(size_FileInfo(info)),
                      Time(lastModified_FileInfo(info)) };

    // Check for overridden status.
    String const overrideName = nativePath + fileStatusSuffix;
    if (fileExistsCStr_FileInfo(overrideName))
    {
        if (std::ifstream f{overrideName.c_str()})
        {
            st.modifiedAt = Time::fromText(Block::readAll(f), Time::ISOFormat);
        }
    }
    return st;
}

void DirectoryFeed::setFileModifiedTime(NativePath const &nativePath, Time const &modifiedAt)
{
    const String overrideName = nativePath + fileStatusSuffix;
    if (!modifiedAt.isValid())
    {
        remove(overrideName);
        return;
    }
    if (std::ofstream f{overrideName.c_str(), std::ios::trunc})
    {
        const String mod = modifiedAt.asText(Time::ISOFormat);
        f.write(mod, mod.size());
    }
}

File &DirectoryFeed::manuallyPopulateSingleFile(NativePath const &nativePath,
                                                Folder &parentFolder) // static
{
    const bool isExisting = nativePath.exists();
    Folder *   parent     = &parentFolder;

    File::Status status;
    if (isExisting)
    {
        status = fileStatus(nativePath);
    }
    else
    {
        status.modifiedAt = Time(); // file being created now
    }

    // If we're populating a .pack, the possible container .packs must be included as
    // parent folders (in structure only, not all their contents). Otherwise the .pack
    // identifier would not be the same.

    if (parentFolder.extension() != ".pack" &&
        nativePath.fileName().fileNameExtension() == ".pack")
    {
        // Extract the portion of the path containing the parent .packs.
        int const last = nativePath.segmentCount() - 1;
        Rangei packRange(last, last);
        while (packRange.start > 0 &&
               nativePath.segment(packRange.start - 1).toString()
               .endsWith(".pack", CaseInsensitive))
        {
            packRange.start--;
        }
        if (!packRange.isEmpty())
        {
            parent = &FS::get().makeFolder(parentFolder.path() /
                                           nativePath.subPath(packRange).withSeparators('/'),
                                           FS::DontInheritFeeds);
        }
    }

    const String newFilePath = parent->path() / nativePath.fileName();

    if (status.type() == File::Type::File)
    {
        parent->clear();
        parent->clearFeeds();

        auto *feed = new DirectoryFeed(nativePath.fileNamePath());
        feed->setNamePattern(nativePath.fileName());
        parent->attach(feed);
        if (isExisting)
        {
            parent->populate();
        }
        else
        {
            parent->replaceFile(nativePath.fileName());
        }
        return FS::locate<File>(newFilePath);
    }
    else
    {
        return FS::get().makeFolderWithFeed(newFilePath,
                                            new DirectoryFeed(nativePath),
                                            Folder::PopulateFullTree,
                                            FS::DontInheritFeeds | FS::PopulateNewFolder);
    }
}

} // namespace de
