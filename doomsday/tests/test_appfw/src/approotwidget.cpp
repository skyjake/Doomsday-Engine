/** @file approotwidget.cpp  Application's root widget.
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

#include "approotwidget.h"
#include "mainwindow.h"
#include "testapp.h"

using namespace de;

DE_PIMPL(AppRootWidget)
{
    Impl(Public *i) : Base(i) {}
};

AppRootWidget::AppRootWidget(GLWindow *window)
    : GuiRootWidget(window), d(new Impl(this))
{}

MainWindow &AppRootWidget::window()
{
    return GuiRootWidget::window().as<MainWindow>();
}

void AppRootWidget::addOnTop(GuiWidget *widget)
{
    // The window knows what is the correct top to add to.
    window().addOnTop(widget);
}

void AppRootWidget::handleEventAsFallback(const Event &/*event*/)
{
    // Handle event at global level, if applicable.
}
