/** @file homemenuwidget.h  Menu for the Home.
 *
 * @authors Copyright (c) 2016 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DENG_CLIENT_UI_HOME_HOMEMENUWIDGET_H
#define DENG_CLIENT_UI_HOME_HOMEMENUWIDGET_H

#include <de/MenuWidget>

/**
 * Menu for items in Home columns.
 *
 * Handles common selection and focusing logic.
 */
class HomeMenuWidget : public de::MenuWidget
{
    Q_OBJECT

public:
    HomeMenuWidget(de::String const &name = "");

    void unselectAll();

    /**
     * Returns the selected widget's index number in the list of menu children.
     */
    int selectedIndex() const;

    void setSelectedIndex(int index);

protected slots:
    void mouseActivityInItem();

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_UI_HOME_HOMEMENUWIDGET_H
