/** @file fx/bloom.h  Bloom camera lens effect.
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

#ifndef DE_CLIENT_FX_BLOOM_H
#define DE_CLIENT_FX_BLOOM_H

#include "render/consoleeffect.h"

namespace fx {

/**
 * Draws a bloom effect that spreads out light from bright pixels. The brightness
 * threshold and bloom intensity are configurable.
 */
class Bloom : public ConsoleEffect
{
public:
    Bloom(int console);

    void glInit();
    void glDeinit();

    void draw();

    static void consoleRegister();
    static bool isEnabled();
    static float intensity();

private:
    DE_PRIVATE(d)
};

} // namespace fx

#endif // DE_CLIENT_FX_BLOOM_H
