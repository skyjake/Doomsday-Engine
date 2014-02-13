/** @file basewindow.cpp  Abstract base class for application windows.
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/BaseWindow"
#include "de/WindowTransform"

namespace de {

DENG2_PIMPL(BaseWindow)
{
    WindowTransform defaultXf; ///< Used by default (doesn't apply any transformation).
    WindowTransform *xf;

    Instance(Public *i)
        : Base(i)
        , defaultXf(self)
        , xf(&defaultXf)
    {}
};

BaseWindow::BaseWindow() : d(new Instance(this))
{}

BaseWindow::~BaseWindow()
{}

void BaseWindow::setTransform(WindowTransform &xf)
{
    d->xf = &xf;
}

void de::BaseWindow::useDefaultTransform()
{
    d->xf = &d->defaultXf;
}

WindowTransform &BaseWindow::transform()
{
    DENG2_ASSERT(d->xf != 0);
    return *d->xf;
}

} // namespace de
