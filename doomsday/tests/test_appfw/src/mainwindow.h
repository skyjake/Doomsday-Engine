/** @file mainwindow.h  The main window.
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <de/basewindow.h>

#include "approotwidget.h"

class MainWindow : public de::BaseWindow
{
public:
    MainWindow(const de::String &id = "main");

    de::GuiRootWidget &root() override;
    de::Vec2f windowContentSize() const override;

    void addOnTop(de::GuiWidget *widget);
    void drawWindowContent() override;
    void preDraw() override;
    void postDraw() override;

protected:
    void windowAboutToClose() override;

private:
    DE_PRIVATE(d)
};

#endif // MAINWINDOW_H
