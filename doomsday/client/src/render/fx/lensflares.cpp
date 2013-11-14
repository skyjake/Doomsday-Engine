/** @file lensflares.cpp
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "render/fx/lensflares.h"
#include "gl/gl_main.h"

#include <de/concurrency.h>
#include <de/Shared>
#include <de/Log>

using namespace de;

namespace fx {

/**
 * Shared GL resources for rendering lens flares.
 */
struct FlareData
{
    FlareData()
    {
        DENG_ASSERT_IN_MAIN_THREAD();
        DENG_ASSERT_GL_CONTEXT_ACTIVE();
    }

    ~FlareData()
    {
        DENG_ASSERT_IN_MAIN_THREAD();
        DENG_ASSERT_GL_CONTEXT_ACTIVE();

        LOG_DEBUG("Releasing shared data");
    }
};

DENG2_PIMPL(LensFlares)
{
    typedef Shared<FlareData> SharedFlareData;
    SharedFlareData *res;

    Instance(Public *i) : Base(i), res(0)
    {}

    ~Instance()
    {
        DENG2_ASSERT(res == 0); // should have been deinited
        releaseRef(res);
    }

    void glInit()
    {
        // Acquire a reference to the shared flare data.
        res = SharedFlareData::hold();
    }

    void glDeinit()
    {
        releaseRef(res);
    }
};

LensFlares::LensFlares(int console) : ConsoleEffect(console), d(new Instance(this))
{}

void LensFlares::glInit()
{
    LOG_AS("fx::LensFlares");

    ConsoleEffect::glInit();
    d->glInit();
}

void LensFlares::glDeinit()
{
    LOG_AS("fx::LensFlares");

    d->glDeinit();
    ConsoleEffect::glDeinit();
}

void LensFlares::draw()
{

}

} // namespace fx

DENG2_SHARED_INSTANCE(fx::FlareData)
