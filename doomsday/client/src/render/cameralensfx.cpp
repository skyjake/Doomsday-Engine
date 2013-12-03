/** @file cameralensfx.cpp  Camera lens effects.
 *
 * Renders camera lens effects, i.e., special effects applied to a "raw" world
 * frame. ConsoleEffect-derived isntances are put onto a stack; each console
 * has its own effect stack.
 *
 * Given the following stack of effects:
 * - A
 * - B
 * - C
 *
 * The following sequence of methods is called during the rendering of a frame:
 *  1. A.beginFrame
 *  2. B.beginFrame
 *  3. C.beginFrame
 *  4. A.draw
 *  5. B.draw
 *  6. C.draw
 *  7. C.endFrame   <-- reverse order
 *  8. B.endFrame
 *  9. A.endFrame
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
#include "con_main.h"

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

#define IDX_LENS_FLARES         2
#define IDX_POST_PROCESSING     3

D_CMD(PostFx)
{
    int console = String(argv[1]).toInt();
    String const shader = argv[2];
    TimeDelta const span = (argc == 4? String(argv[3]).toFloat() : 0);

    if(console < 0 || console >= DDMAXPLAYERS)
    {
        LOG_WARNING("Invalid console %i") << console;
        return false;
    }

    fx::PostProcessing *post =
            static_cast<fx::PostProcessing *>(fxConsole[console].effects[IDX_POST_PROCESSING]);

    // Special case to clear out the current shader.
    if(shader == "none")
    {
        post->fadeOut(span);
        return true;
    }

    post->fadeInShader(shader, span);
    return true;
}

void LensFx_Register()
{
    C_CMD("postfx", "is", PostFx);
    C_CMD("postfx", "isf", PostFx);
}

void LensFx_Init()
{
    for(int i = 0; i < DDMAXPLAYERS; ++i)
    {
        ConsoleEffectStack &stack = fxConsole[i];
        stack.effects.append(new fx::ColorFilter(i));
        stack.effects.append(new fx::Vignette(i));
        stack.effects.append(new fx::LensFlares(i));
        stack.effects.append(new fx::PostProcessing(i)); // IDX_POST_PROCESSING
    }
}

void LensFx_Shutdown()
{
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

void LensFx_MarkLightVisibleInFrame(ILightSource const &lightSource)
{
    ConsoleEffectStack::EffectList const &effects = fxConsole[fxFramePlayerNum].effects;

    static_cast<fx::LensFlares *>(effects.at(IDX_LENS_FLARES))->
            markLightPotentiallyVisibleForCurrentFrame(&lightSource);
}
