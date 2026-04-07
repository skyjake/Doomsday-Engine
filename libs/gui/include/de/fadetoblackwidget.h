/** @file fadetoblackwidget.h  Fade to/from black.
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

#ifndef LIBAPPFW_FADETOBLACKWIDGET
#define LIBAPPFW_FADETOBLACKWIDGET

#include "de/labelwidget.h"
#include <de/time.h>

namespace de {

/**
 * Fade to/from black.
 *
 * @ingroup guiWidgets
 */
class LIBGUI_PUBLIC FadeToBlackWidget : public LabelWidget
{
public:
    FadeToBlackWidget();

    void initFadeFromBlack(TimeSpan span);
    void initFadeToBlack(TimeSpan span);

    void start(TimeSpan delay = 0.0);
    void pause();
    void resume();
    void cancel();
    void disposeIfDone();

    bool isStarted() const;
    bool isDone() const;

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_FADETOBLACKWIDGET

