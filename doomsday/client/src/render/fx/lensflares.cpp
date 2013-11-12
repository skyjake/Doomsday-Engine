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
#include <de/Counted>
#include <de/Log>

using namespace de;

namespace fx {

/**
 * Shared GL resources for rendering lens flares.
 */
struct SharedFlareData : public Counted
{
    static SharedFlareData *instance;

    SharedFlareData()
    {
        DENG_ASSERT_IN_MAIN_THREAD();
        DENG_ASSERT_GL_CONTEXT_ACTIVE();
    }

    ~SharedFlareData()
    {
        DENG_ASSERT_IN_MAIN_THREAD();
        DENG_ASSERT_GL_CONTEXT_ACTIVE();

        LOG_DEBUG("Releasing shared data");
        instance = 0;
    }

    static SharedFlareData *hold()
    {
        if(!instance)
        {
            instance = new SharedFlareData;
            LOG_DEBUG("Allocated shared data");
        }
        else
        {
            // Hold a reference to the shared data.
            instance->ref<Counted>();
        }
        return instance;
    }
};

SharedFlareData *SharedFlareData::instance = 0;

DENG2_PIMPL(LensFlares)
{
    SharedFlareData *res;

    Instance(Public *i) : Base(i), res(0)
    {
    }

    ~Instance()
    {
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

    d->glInit();
}

void LensFlares::glDeinit()
{
    LOG_AS("fx::LensFlares");

    d->glDeinit();
}

void LensFlares::draw(Rectanglei const &viewRect)
{

}

} // namespace fx
