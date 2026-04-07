/** @file nativepathwidget.h  Widget for selecting a native path.
 *
 * @authors Copyright (c) 2014-2018 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DE_CLIENT_UI_NATIVEPATHWIDGET_H
#define DE_CLIENT_UI_NATIVEPATHWIDGET_H

#include <de/auxbuttonwidget.h>
#include <de/filedialog.h>

/**
 * Console variable that contains a native path.
 */
class NativePathWidget : public de::AuxButtonWidget
{
public:
    DE_AUDIENCE(UserChange, void pathChangedByUser(NativePathWidget &))

public:
    NativePathWidget();

    /**
     * Sets all the file types that can be selected using the widget.
     * The default is to show all files.
     *
     * @param filters  Allowed file types.
     */
    void setFilters(const de::FileDialog::FileTypes &filters);

    /**
     * Sets the text that is shown as the current selection when nothing has been
     * selected.
     *
     * @param blankText  Blank text placeholder.
     */
    void setBlankText(const de::String &blankText);

    void setPrompt(const de::String &promptText);

    void setPath(const de::NativePath &path);

    de::NativePath path() const;

    void chooseUsingNativeFileDialog();

    void clearPath();

    void showActionsPopup();

private:
    DE_PRIVATE(d)
};

#endif // DE_CLIENT_UI_NATIVEPATHWIDGET_H
