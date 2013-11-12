/** @file cameralensfx.cpp  Camera lens effects.
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

#include "render/cameralensfx.h"
#include "render/rend_main.h"
#include "render/viewports.h"
#include "render/fx/colorfilter.h"
#include "render/fx/lensflares.h"
#include "render/fx/vignette.h"

#include <de/libdeng2.h>
#include <de/Rectangle>
#include <QList>

using namespace de;

static int fxFramePlayerNum; ///< Player view currently being drawn.

struct ConsoleEffectStack
{
    /// Dynamic stack of effects. Used currently as a fixed array, though.
    QList<ConsoleEffect *> effects;

    ~ConsoleEffectStack() {
        clear();
    }

    void clear() {
        qDeleteAll(effects);
        effects.clear();
    }
};

static ConsoleEffectStack fxConsole[DDMAXPLAYERS];

void LensFx_Init()
{
    for(int i = 0; i < DDMAXPLAYERS; ++i)
    {
        ConsoleEffectStack &stack = fxConsole[i];
        stack.effects.append(new fx::ColorFilter(i));
        stack.effects.append(new fx::Vignette(i));
        stack.effects.append(new fx::LensFlares(i));

        foreach(ConsoleEffect *effect, stack.effects)
        {
            effect->glInit();
        }
    }
}

void LensFx_Shutdown()
{
    for(int i = 0; i < DDMAXPLAYERS; ++i)
    {
        foreach(ConsoleEffect *effect, fxConsole[i].effects)
        {
            effect->glDeinit();
        }

        fxConsole[i].clear();
    }
}

void LensFx_BeginFrame(int playerNum)
{
    fxFramePlayerNum = playerNum;
}

void LensFx_EndFrame()
{
    viewdata_t const *vd = R_ViewData(fxFramePlayerNum);
    Rectanglei const viewRect(vd->window.origin.x,
                              vd->window.origin.y,
                              vd->window.size.width,
                              vd->window.size.height);

    // Draw all the effects for this console.
    foreach(ConsoleEffect *effect, fxConsole[fxFramePlayerNum].effects)
    {
        effect->draw(viewRect);
    }
}
