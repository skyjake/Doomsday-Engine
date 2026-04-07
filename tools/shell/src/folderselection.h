/** @file folderselection.h  Widget for selecting a folder.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef FOLDERSELECTION_H
#define FOLDERSELECTION_H

#include <de/guiwidget.h>
#include <de/nativepath.h>

/**
 * Widget for selecting a folder.
 */
class FolderSelection : public de::GuiWidget
{
public:
    explicit FolderSelection(const de::String &prompt);

    void setPath(const de::NativePath &path);
    void setEnabled(bool yes);
    void setDisabled(bool yes) { setEnabled(!yes); }

    de::NativePath path() const;

    DE_AUDIENCE(Selection, void folderSelected(const de::NativePath &))

    void selectFolder();

private:
    DE_PRIVATE(d)
};

#endif // FOLDERSELECTION_H
