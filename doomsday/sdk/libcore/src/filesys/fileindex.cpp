/** @file fileindex.cpp  Index for looking up files of specific type.
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/FileIndex"
#include "de/ReadWriteLockable"
#include "de/PackageLoader"
#include "de/App"
#include "de/LogBuffer"

namespace de {

DENG2_PIMPL(FileIndex), public ReadWriteLockable
{
    IPredicate const *predicate;
    Index index;

    Impl(Public *i)
        : Base(i)
        , predicate(0)
    {
        // File operations may occur in several threads.
        audienceForAddition.setAdditionAllowedDuringIteration(true);
        audienceForRemoval .setAdditionAllowedDuringIteration(true);
    }

    static String indexedName(File const &file)
    {
        String name = file.name().lower();

        // Ignore the package version in the indexed names.
        if (name.endsWith(".pack"))
        {
            name = Package::split(name.fileNameWithoutExtension()).first + ".pack";
        }

        return name;
    }

    void add(File const &file)
    {
        DENG2_GUARD_WRITE(this);

        index.insert(std::pair<String, File *>(indexedName(file), const_cast<File *>(&file)));
    }

    void remove(File const &file)
    {
        DENG2_GUARD_WRITE(this);

        if (index.empty())
        {
            return;
        }

        // Look up the ones that might be this file.
        IndexRange range = index.equal_range(indexedName(file));

        for (Index::iterator i = range.first; i != range.second; ++i)
        {
            if (i->second == &file)
            {
                // This is the one to deindex.
                index.erase(i);
                break;
            }
        }
    }

    void findPartialPath(String const &path, FoundFiles &found) const
    {
        String baseName = path.fileName().lower();
        String dir      = path.fileNamePath().lower();

        if (!dir.empty() && !dir.beginsWith("/"))
        {
            // Always begin with a slash. We don't want to match partial folder names.
            dir = "/" + dir;
        }

        DENG2_GUARD_READ(this);

        ConstIndexRange range = index.equal_range(baseName);
        for (Index::const_iterator i = range.first; i != range.second; ++i)
        {
            File *file = i->second;
            if (file->path().fileNamePath().endsWith(dir, String::CaseInsensitive))
            {
                found.push_back(file);
            }
        }
    }

    DENG2_PIMPL_AUDIENCE(Addition)
    DENG2_PIMPL_AUDIENCE(Removal)
};

DENG2_AUDIENCE_METHOD(FileIndex, Addition)
DENG2_AUDIENCE_METHOD(FileIndex, Removal)

FileIndex::FileIndex() : d(new Impl(this))
{}

void FileIndex::setPredicate(IPredicate const &predicate)
{
    d->predicate = &predicate;
}

bool FileIndex::maybeAdd(File const &file)
{
    if (d->predicate && !d->predicate->shouldIncludeInIndex(file))
    {
        return false;
    }

    d->add(file);

    // Notify audience.
    DENG2_FOR_AUDIENCE2(Addition, i)
    {
        i->fileAdded(file, *this);
    }

    return true;
}

void FileIndex::remove(File const &file)
{
    d->remove(file);

    // Notify audience.
    DENG2_FOR_AUDIENCE2(Removal, i)
    {
        i->fileRemoved(file, *this);
    }
}

int FileIndex::size() const
{
    DENG2_GUARD_READ(d);
    return int(d->index.size());
}

static bool fileNotInAnyLoadedPackage(File *file)
{
    String const identifier = Package::identifierForContainerOfFile(*file);
    return !App::packageLoader().isLoaded(identifier);
}

void FileIndex::findPartialPath(String const &path, FoundFiles &found, Behavior behavior) const
{
    d->findPartialPath(path, found);

    if (behavior == FindOnlyInLoadedPackages)
    {
        found.remove_if(fileNotInAnyLoadedPackage);
    }
}

void FileIndex::findPartialPath(Folder const &rootFolder, String const &path,
                                FoundFiles &found, Behavior behavior) const
{
    findPartialPath(path, found, behavior);

    // Remove any matches outside the given root.
    found.remove_if([&rootFolder] (File *file) {
        return !file->hasAncestor(rootFolder);
    });
}

void FileIndex::findPartialPath(String const &packageId, String const &path,
                                FoundFiles &found) const
{
    // We can only look in Folder-like packages.
    Package const &pkg = App::packageLoader().package(packageId);
    if (is<Folder>(pkg.file()))
    {
        findPartialPath(pkg.root(), path, found, FindInEntireIndex);

        // Remove any matches not in the given package.
        found.remove_if([&packageId](File *file) {
            return Package::identifierForContainerOfFile(*file) != packageId;
        });
    }
}

int FileIndex::findPartialPathInPackageOrder(String const &path, FoundFiles &found, Behavior behavior) const
{
    findPartialPath(path, found, behavior);
    App::packageLoader().sortInPackageOrder(found);
    return int(found.size());
}

FileIndex::Index::const_iterator FileIndex::begin() const
{
    return d.getConst()->index.begin();
}

FileIndex::Index::const_iterator FileIndex::end() const
{
    return d.getConst()->index.end();
}

void FileIndex::print() const
{
    DENG2_GUARD_READ(d);
    for (auto i = begin(); i != end(); ++i)
    {
        LOG_TRACE("\"%s\": ", i->first << i->second->description());
    }
}

QList<File *> FileIndex::files() const
{
    DENG2_GUARD_READ(d);
    QList<File *> list;
    for (auto i = begin(); i != end(); ++i)
    {
        list.append(i->second);
    }
    return list;
}

} // namespace de
