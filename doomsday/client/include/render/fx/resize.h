/** @file fx/resize.h  Change the size (pixel density) of the view.
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

#ifndef DENG_CLIENT_FX_RESIZE_H
#define DENG_CLIENT_FX_RESIZE_H

#include "render/consoleeffect.h"
#include <de/Time>

namespace fx {

/**
 * Post-processing of rendered camera lens frames. Maintains an offscreen
 * render target and provides a way to draw it back to the regular target
 * with shader effects applied.
 */
class Resize : public ConsoleEffect
{
public:
    Resize(int console);

    /**
     * Determines whether the effect is active. If it isn't, it can be skipped
     * altogether when post processing a frame.
     */
    bool isActive() const;

    void glInit();
    void glDeinit();

    void beginFrame();
    void endFrame();

private:
    DENG2_PRIVATE(d)
};

} // namespace fx

#endif // DENG_CLIENT_FX_RESIZE_H
