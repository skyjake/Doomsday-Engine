/*
 * The Doomsday Engine Project
 *
 * Copyright (c) 2009, 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "window.h"
#include "surface.h"
#include "video.h"
#include "rules.h"
#include <QDebug>

using namespace de;

Window::Window(const QGLFormat& format, QWidget* parent, const QGLWidget* shareWidget)
    : GLWindowSurface(format, parent, shareWidget)
{
    _widthRule = new ConstantRule(width(), this);
    _heightRule = new ConstantRule(height(), this);

    // Define rules for the root visual's placement.
    _root.setRect(new RectangleRule(new Rule(), new Rule(), _widthRule, _heightRule));
}

Window::~Window()
{}

void Window::setSelectedFlags(Flags selectedFlags, bool set)
{
    Flags flags = _flags;
    if(set)
    {
        flags |= selectedFlags;
    }
    else
    {
        flags &= ~selectedFlags;
    }
    setFlags(flags);
}

void Window::setFlags(Flags allFlags)
{
    _flags = allFlags;
}

void Window::surfaceResized(const QSize& size)
{
    qDebug() << "Window: Surface resized" << size;

    // Update the visual layout.
    _widthRule->set(size.width());
    _heightRule->set(size.height());
}

void Window::draw()
{
    qDebug() << "Window: Drawing, root placement:" << _root.rect().asText();

    // Draw all the visuals.
    _root.draw();
}
