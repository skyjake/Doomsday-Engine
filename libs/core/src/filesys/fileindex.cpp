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

#include "de/fileindex.h"
#include "de/packageloader.h"
#include "de/app.h"
#include "de/logbuffer.h"

namespace de {

DE_PIMPL(FileIndex), public Lockable
{
    const IPredicate *predicate;
    Index index;

    Impl(Public *i)
        : Base(i)
        , predicate(nullptr)
    {
        // File operations may occur in several threads.
        audienceForAddition.setAdditionAllowedDuringIteration(true);
        audienceForRemoval .setAdditionAllowedDuringIteration(true);
    }

    static String indexedName(const File &file)
    {
        String name = file.name();

        DE_ASSERT(name.lower() == name);

        // Ignore the package version in the indexed names.
        if (name.endsWith(".pack"))
        {
            name = Package::split(name.fileNameWithoutExtension()).first + ".pack";
        }

        return name;
    }

    void add(const File &file)
    {
        DE_GUARD(this);
        const String name = indexedName(file);
        DE_ASSERT(!name.isEmpty());
        index.insert(std::pair<String, File *>(name, const_cast<File *>(&file)));
    }

    void remove(const File &file)
    {
        DE_GUARD(this);

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

    void findPartialPath(const String &path, FoundFiles &found) const
    {
        String baseName = path.fileName();
        String dir      = path.fileNamePath();

        if (!dir.empty() && !dir.beginsWith("/"))
        {
            // Always begin with a slash. We don't want to match partial folder names.
            dir = "/" + dir;
        }

        DE_GUARD(this);

        auto range = index.equal_range(baseName.lower());
        for (Index::const_iterator i = range.first; i != range.second; ++i)
        {
            File *file = i->second;
            if (file->path().fileNamePath().endsWith(dir, CaseInsensitive))
            {
                found.push_back(file);
            }
        }
    }

    DE_PIMPL_AUDIENCE(Addition)
    DE_PIMPL_AUDIENCE(Removal)
};

DE_AUDIENCE_METHOD(FileIndex, Addition)
DE_AUDIENCE_METHOD(FileIndex, Removal)

FileIndex::FileIndex() : d(new Impl(this))
{}

void FileIndex::setPredicate(const IPredicate &predicate)
{
    d->predicate = &predicate;
}

bool FileIndex::maybeAdd(const File &file)
{
    if (d->predicate && !d->predicate->shouldIncludeInIndex(file))
    {
        return false;
    }

    d->add(file);

    // Notify audience.
    DE_NOTIFY(Addition, i)
    {
        i->fileAdded(file, *this);
    }

    return true;
}

void FileIndex::remove(const File &file)
{
    d->remove(file);

    // Notify audience.
    DE_NOTIFY(Removal, i)
    {
        i->fileRemoved(file, *this);
    }
}

int FileIndex::size() const
{
    DE_GUARD(d);
    return int(d->index.size());
}

static bool fileNotInAnyLoadedPackage(File *file)
{
    const String identifier = Package::identifierForContainerOfFile(*file);
    return !App::packageLoader().isLoaded(identifier);
}

void FileIndex::findPartialPath(const String &path, FoundFiles &found, Behavior behavior) const
{
    d->findPartialPath(path, found);

    if (behavior == FindOnlyInLoadedPackages)
    {
        found.remove_if(fileNotInAnyLoadedPackage);
    }
}

void FileIndex::findPartialPath(const Folder &rootFolder, const String &path,
                                FoundFiles &found, Behavior behavior) const
{
    findPartialPath(path, found, behavior);

    // Remove any matches outside the given root.
    found.remove_if([&rootFolder] (File *file) {
        return !file->hasAncestor(rootFolder);
    });
}

void FileIndex::findPartialPath(const String &packageId, const String &path,
                                FoundFiles &found) const
{
    // We can only look in Folder-like packages.
    const Package &pkg = App::packageLoader().package(packageId);
    if (is<Folder>(pkg.file()))
    {
        findPartialPath(pkg.root(), path, found, FindInEntireIndex);

        // Remove any matches not in the given package.
        found.remove_if([&packageId](File *file) {
            return Package::identifierForContainerOfFile(*file) != packageId;
        });
    }
}

int FileIndex::findPartialPathInPackageOrder(const String &path, FoundFiles &found, Behavior behavior) const
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
    DE_GUARD(d);
    for (auto i = begin(); i != end(); ++i)
    {
        LOG_TRACE("\"%s\": ", i->first << i->second->description());
    }
}

List<File *> FileIndex::files() const
{
    DE_GUARD(d);
    List<File *> list;
    for (auto i = begin(); i != end(); ++i)
    {
        list.append(i->second);
    }
    return list;
}

} // namespace de
