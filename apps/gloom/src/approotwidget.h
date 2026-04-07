/** @file approotwidget.h  Application's root widget.
 *
 * @authors Copyright (c) 2014-2018 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef APPROOTWIDGET_H
#define APPROOTWIDGET_H

#include <de/guirootwidget.h>
#include <de/glwindow.h>

class MainWindow;

/**
 * GUI root widget tailored for the application.
 */
class AppRootWidget : public de::GuiRootWidget
{
public:
    AppRootWidget(de::GLWindow *window = 0);

    MainWindow &window();

//    void dispatchLatestMousePosition();
    void handleEventAsFallback(const de::Event &event);

private:
    DE_PRIVATE(d)
};

#endif // APPROOTWIDGET_H
