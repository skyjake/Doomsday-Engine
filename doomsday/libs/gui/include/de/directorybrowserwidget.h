/** @file directorybrowserwidget.h
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

#include "de/browserwidget.h"
#include "de/directorytreedata.h"

namespace de {

class DirectoryBrowserWidget : public BrowserWidget
{
public:
    enum Flag {
        ShowFiles       = 0x1,
        ShowHiddenFiles = 0x2,
    };

public:
    DirectoryBrowserWidget(Flags flags = ShowFiles, const String &name = "dirbrowser");

    NativePath       currentDirectory() const;
    List<NativePath> selectedPaths() const;

    DE_AUDIENCE(Selection, void itemSelected(DirectoryBrowserWidget &, const DirectoryItem &))

private:
    DE_PRIVATE(d)
};

} // namespace de
