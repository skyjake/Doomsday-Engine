/** @file cvarnativepathwidget.h  Console variable native path widget.
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DENG_CLIENT_CVARNATIVEPATHWIDGET_H
#define DENG_CLIENT_CVARNATIVEPATHWIDGET_H

#include <de/AuxButtonWidget>
#include "icvarwidget.h"

/**
 * Console variable that contains a native path.
 */
class CVarNativePathWidget : public de::AuxButtonWidget, public ICVarWidget
{
    Q_OBJECT

public:
    CVarNativePathWidget(char const *cvarPath);

    /**
     * Sets all the file types that can be selected using the widget. Each entry in
     * the list should be formatted as "Description (*.ext *.ext2)".
     *
     * The default is "All files (*)".
     *
     * @param filters  Allowed file types.
     */
    void setFilters(de::StringList const &filters);

    /**
     * Sets the text that is shown as the current selection when nothing has been
     * selected.
     *
     * @param text  Blank text placeholder.
     */
    void setBlankText(de::String const &text);

    char const *cvarPath() const;

public slots:
    void updateFromCVar();
    void chooseUsingNativeFileDialog();

protected slots:
    void setCVarValueFromWidget();

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_CVARNATIVEPATHWIDGET_H
