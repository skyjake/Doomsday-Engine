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
    NativePath dir;
    ui::ListDataT<DirectoryItem> items;

    Impl(Public *i) : Base(i)
    {}

    void populate(const NativePath &path)
    {
        // Get rid of the previous contents.
        items.clear();

        // Populate a folder with the directory contents.
        dir = path;
        Folder folder(path.fileName());
        folder.attach(new DirectoryFeed(dir));
        folder.populate(Folder::PopulateOnlyThisFolder | Folder::DisableNotification);

        // Create corresponding data items.
        for (const auto &entry : folder.contents())
        {
            items << new DirectoryItem(entry.first, entry.second->status(), dir);
        }

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
    if (NativePath(path) != d->dir)
    {
        d->populate(path);
    }
    return d->items;
}

} // namespace de
