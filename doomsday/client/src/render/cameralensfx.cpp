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
#include "render/fx/postprocessing.h"
#include "render/fx/vignette.h"

#include "ui/clientwindow.h"

#include <de/libdeng2.h>
#include <de/Rectangle>
#include <de/Drawable>
#include <de/GLTarget>
#include <QList>

using namespace de;

static int fxFramePlayerNum; ///< Player view currently being drawn.

struct ConsoleEffectStack
{
    /// Dynamic stack of effects. Used currently as a fixed array, though.
    typedef QList<ConsoleEffect *> EffectList;
    EffectList effects;

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
    //postProc.glInit();

    for(int i = 0; i < DDMAXPLAYERS; ++i)
    {
        ConsoleEffectStack &stack = fxConsole[i];
        stack.effects.append(new fx::ColorFilter(i));
        stack.effects.append(new fx::Vignette(i));
        stack.effects.append(new fx::LensFlares(i));
        stack.effects.append(new fx::PostProcessing(i));

        /*foreach(ConsoleEffect *effect, stack.effects)
        {
            effect->glInit();
        }*/
    }
}

void LensFx_Shutdown()
{
    //postProc.glDeinit();

    LensFx_GLRelease();

    for(int i = 0; i < DDMAXPLAYERS; ++i)
    {
        fxConsole[i].clear();
    }
}

void LensFx_GLRelease()
{
    for(int i = 0; i < DDMAXPLAYERS; ++i)
    {
        foreach(ConsoleEffect *effect, fxConsole[i].effects)
        {
            if(effect->isInited())
            {
                effect->glDeinit();
            }
        }
    }
}

void LensFx_BeginFrame(int playerNum)
{
    fxFramePlayerNum = playerNum;

    ConsoleEffectStack::EffectList const &effects = fxConsole[fxFramePlayerNum].effects;

    // Initialize these effects if they currently are not.
    foreach(ConsoleEffect *effect, effects)
    {
        if(!effect->isInited())
        {
            effect->glInit();
        }
    }

    foreach(ConsoleEffect *effect, effects)
    {
        effect->beginFrame();
    }
}

void LensFx_EndFrame()
{
    ConsoleEffectStack::EffectList const &effects = fxConsole[fxFramePlayerNum].effects;

    foreach(ConsoleEffect *effect, effects)
    {
        effect->draw();
    }

    for(int i = effects.size() - 1; i >= 0; --i)
    {
        effects.at(i)->endFrame();
    }
}
