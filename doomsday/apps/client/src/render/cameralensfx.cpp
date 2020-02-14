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

#include "de_platform.h"

#include "render/cameralensfx.h"
#include "render/rend_main.h"
#include "render/viewports.h"
#include "render/fx/bloom.h"
#include "render/fx/colorfilter.h"
#include "render/fx/lensflares.h"
#include "render/fx/ramp.h"
#include "render/fx/vignette.h"
#include "world/p_players.h"

#include "clientapp.h"
#include "ui/clientwindow.h"
#include "ui/viewcompositor.h"

#include <doomsday/console/cmd.h>
#include <de/libcore.h>
#include <de/Rectangle>
#include <de/Drawable>
#include <de/GLFramebuffer>

using namespace de;

static int fxFramePlayerNum; ///< Player view currently being drawn.

//#define IDX_LENS_FLARES         2

void LensFx_Init()
{
    for (int i = 0; i < DDMAXPLAYERS; ++i)
    {
        ConsoleEffectStack &stack = DD_Player(i)->fxStack();

        if (ClientApp::hasClassicWorld()) // Gloom does its own HDR bloom.
        {
            stack.effects << new fx::Bloom(i);
        }

        stack.effects << new fx::Vignette(i)
//                << new fx::LensFlares(i)        // IDX_LENS_FLARES
                << new fx::ColorFilter(i)
                << new fx::Ramp(i);
    }
}

void LensFx_Shutdown()
{
    LensFx_GLRelease();

    for (int i = 0; i < DDMAXPLAYERS; ++i)
    {
        DD_Player(i)->fxStack().clear();
    }
}

void LensFx_GLRelease()
{
    for (int i = 0; i < DDMAXPLAYERS; ++i)
    {
        for (ConsoleEffect *effect : DD_Player(i)->fxStack().effects)
        {
            if (effect->isInited())
            {
                effect->glDeinit();
            }
        }
    }
}

void LensFx_Draw(int playerNum)
{
    ClientPlayer &player = ClientApp::player(playerNum);
    fxFramePlayerNum = playerNum;

    // Must first resolve multisampling because everything except the 3D world itself
    // is drawn without multisampling. This will make the attached textures available
    // for use in effects.
    player.viewCompositor().gameView().resolveSamples();

    // TODO: Refactor the MS/resolved switch; it should encapsulate only the 3D world
    // rendering section.

    // Now that we've resolved multisampling, further rendering must be done without it.
    GLState::push()
            .setTarget(player.viewCompositor().gameView().resolvedFramebuffer());

    const auto &effects = player.fxStack().effects;

    // Initialize these effects if they currently are not.
    for (ConsoleEffect *effect : effects)
    {
        if (!effect->isInited())
        {
            effect->glInit();
        }
    }

    for (ConsoleEffect *effect : effects)
    {
        effect->beginFrame();
    }

    for (ConsoleEffect *effect : effects)
    {
        effect->draw();
    }

    for (int i = effects.sizei() - 1; i >= 0; --i)
    {
        effects.at(i)->endFrame();
    }

    GLState::pop();
}

void LensFx_MarkLightVisibleInFrame(const IPointLightSource &/*lightSource*/)
{
    /*
    const auto &effects = DD_Player(fxFramePlayerNum)->fxStack().effects;

    static_cast<fx::LensFlares *>(effects.at(IDX_LENS_FLARES))->
            markLightPotentiallyVisibleForCurrentFrame(&lightSource);
    */
}
