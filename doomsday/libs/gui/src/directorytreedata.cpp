/** @file directorytreedata.cpp  Native filesystem directory tree.
 *
 * @authors Copyright (c) 2019 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/DirectoryTreeData"
#include "de/ui/ListData"

#include <de/DirectoryFeed>
#include <de/Folder>

namespace de {

DE_PIMPL(DirectoryTreeData)
{
    using DirList = ui::ListDataT<DirectoryItem>;
    Map<NativePath, std::unique_ptr<DirList>> pathItems;

    Impl(Public *i) : Base(i)
    {}

    void populate(const NativePath &path)
    {
        auto found = pathItems.find(path);
        if (found == pathItems.end())
        {
            found = pathItems.insert(path, std::unique_ptr<DirList>(new DirList));
        }
        auto &items = *found->second;

        // Get rid of the previous contents.
        items.clear();

        // Populate a folder with the directory contents.
        Folder folder(path.fileName());
        folder.attach(new DirectoryFeed(path));
        folder.populate(Folder::PopulateOnlyThisFolder | Folder::DisableNotification |
                        Folder::DisableIndexing);

        // Create corresponding data items.
        folder.forContents([&items, &found](String, File &file) {
            const auto native = file.correspondingNativePath();
            items << new DirectoryItem(native.fileName(), file.status(), found->first);
            return LoopContinue;
        });

        items.sort();
    }
};

DirectoryTreeData::DirectoryTreeData()
    : d(new Impl(this))
{}

bool DirectoryTreeData::contains(const Path &path) const
{
    return NativePath(path).isDirectory();
}

const ui::Data &DirectoryTreeData::items(const Path &path) const
{
    DE_ASSERT(contains(path));

    const NativePath dir(path);
    if (!d->pathItems.contains(dir))
    {
        d->populate(dir);
    }
    return *d->pathItems[dir];
}

} // namespace de
