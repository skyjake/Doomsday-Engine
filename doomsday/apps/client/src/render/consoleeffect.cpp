/** @file consoleeffect.cpp
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "render/consoleeffect.h"
#include "render/viewports.h"
#include "render/rendersystem.h"
#include "world/p_players.h"
#include "clientapp.h"

using namespace de;

DE_PIMPL_NOREF(ConsoleEffect)
{
    int console;
    bool inited;

    Impl() : console(0), inited(false)
    {}
};

ConsoleEffect::ConsoleEffect(int console) : d(new Impl)
{
    d->console = console;
}

ConsoleEffect::~ConsoleEffect()
{}

int ConsoleEffect::console() const
{
    return d->console;
}

Rectanglei ConsoleEffect::viewRect() const
{
    return Rectanglei::fromSize(R_Console3DViewRect(d->console).size());
}

bool ConsoleEffect::isInited() const
{
    return d->inited;
}

GLShaderBank &ConsoleEffect::shaders() const
{
    return ClientApp::render().shaders();
}

void ConsoleEffect::glInit()
{
    d->inited = true;
}

void ConsoleEffect::glDeinit()
{
    d->inited = false;
}

void ConsoleEffect::beginFrame()
{}

void ConsoleEffect::draw()
{}

void ConsoleEffect::endFrame()
{}
