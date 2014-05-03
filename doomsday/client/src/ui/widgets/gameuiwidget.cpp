/** @file gameuiwidget.cpp  Widget for legacy game UI elements.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2014 Daniel Swanson <danij@dengine.net>
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

#include "ui/widgets/gameuiwidget.h"

#include "de_base.h"
#include "de_console.h"
#include "de_ui.h"
#include "de_render.h"
#include "audio/s_main.h"
#include "network/net_main.h"
#include "gl/gl_main.h"
#include "edit_bias.h"
#include "ui/busyvisual.h"

#include <de/GLState>

using namespace de;

DENG2_PIMPL(GameUIWidget)
{
    Instance(Public *i) : Base(i)
    {}

    void draw()
    {
        if(App_GameLoaded())
        {
            R_RenderViewPorts(HUDLayer);

            UI2_Drawer();

            // Draw any full window game graphics.
            if(gx.DrawWindow)
            {
                Size2Raw dimensions(DENG_GAMEVIEW_WIDTH, DENG_GAMEVIEW_HEIGHT);
                gx.DrawWindow(&dimensions);
            }
        }

        // Draw the widgets of the Shadow Bias Editor (if active).
        SBE_DrawGui();

        /*
         * Draw debug information.
         */
        if(App_WorldSystem().hasMap() && App_WorldSystem().map().hasLightGrid())
        {
            Rend_LightGridVisual(App_WorldSystem().map().lightGrid());
        }
        Net_Drawer();
        S_Drawer();

        DGL_End();
    }
};

GameUIWidget::GameUIWidget() : GuiWidget("game_ui"), d(new Instance(this))
{}

void GameUIWidget::drawContent()
{
    if(isDisabled() || !GL_IsFullyInited())
        return;

    GLState::push().apply();

    /*
    Rectanglei pos;
    if(hasChangedPlace(pos))
    {
        // Automatically update if the widget is resized.
        d->updateSize();
    }*/

    d->draw();

    GLState::considerNativeStateUndefined();
    GLState::pop().apply();
}

