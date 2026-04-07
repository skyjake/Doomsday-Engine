/** @file fadetoblackwidget.cpp  Fade to/from black.
 *
 * @authors Copyright (c) 2015-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/fadetoblackwidget.h"

namespace de {

DE_PIMPL_NOREF(FadeToBlackWidget)
{
    TimeSpan span = 1.0;
    bool started = false;
};

FadeToBlackWidget::FadeToBlackWidget() : d(new Impl)
{
    set(Background(Vec4f(0, 0, 0, 1)));
}

void FadeToBlackWidget::initFadeFromBlack(TimeSpan span)
{
    setOpacity(1);
    d->span = span;
    d->started = false;
}

void FadeToBlackWidget::initFadeToBlack(TimeSpan span)
{
    setOpacity(0);
    d->span = span;
    d->started = false;
}

void FadeToBlackWidget::start(TimeSpan delay)
{
    d->started = true;
    setOpacity(fequal(opacity().target(), 1)? 0 : 1, d->span, delay);
}

void FadeToBlackWidget::pause()
{
    opacityAnimation().pause();
}

void FadeToBlackWidget::resume()
{
    opacityAnimation().resume();
}

void FadeToBlackWidget::cancel()
{
    d->started = true;
    setOpacity(0);
}

bool FadeToBlackWidget::isStarted() const
{
    return d->started;
}

bool FadeToBlackWidget::isDone() const
{
    return isStarted() && opacity().done();
}

void FadeToBlackWidget::disposeIfDone()
{
    if (isDone())
    {
        destroyLater(this);
    }
}

} // namespace de
