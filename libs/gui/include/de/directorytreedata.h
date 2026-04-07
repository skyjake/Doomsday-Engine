/** @file directorytreedata.h  Native filesystem directory tree.
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

#pragma once

#include "de/ui/item.h"
#include "de/ui/treedata.h"
#include <de/file.h>

namespace de {

class DirectoryItem;

class LIBGUI_PUBLIC DirectoryTreeData : public ui::TreeData
{
public:
    DE_ERROR(InvalidDirectoryError);

public:
    DirectoryTreeData();

    void setPopulateFiles(bool files);
    void setPopulateHiddenFiles(bool hiddenFiles);

    // Implements ui::TreeData.
    bool            contains(const Path &path) const override;
    ui::Data &      items(const Path &path) override;
    const ui::Data &items(const Path &path) const override;

private:
    DE_PRIVATE(d)
};

/**
 * Item in the directory tree data model (i.e., file or subdirectory).
 */
class LIBGUI_PUBLIC DirectoryItem : public ui::Item
{
public:
    DirectoryItem(const String &name, const File::Status &status, const Path &directory)
        : ui::Item(DefaultSemantics, name)
        , _status(status)
        , _directory(directory)
    {
        setLabel(name);
    }

    String        name() const { return label(); }
    File::Status  status() const { return _status; }
    Path          path() const { return _directory / name(); }

    inline bool isDirectory() const
    {
        return status().type() == File::Type::Folder;
    }

private:
    File::Status _status;
    const Path & _directory;
};

} // namespace de
