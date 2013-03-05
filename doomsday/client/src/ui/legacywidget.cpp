/** @file legacywidget.cpp
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

#include "de_platform.h"
#include "ui/legacywidget.h"
#include "ui/dd_input.h"
#include "ui/ui_main.h"
#include "ui/ui2_main.h"
#include "ui/busyvisual.h"
#include "dd_def.h"
#include "dd_main.h"
#include "dd_loop.h"
#include "sys_system.h"
#include "map/gamemap.h"
#include "network/net_main.h"
#include "render/r_main.h"
#include "render/rend_list.h"
#include "render/rend_console.h"
#include "audio/s_main.h"
#include "gl/sys_opengl.h"
#include "gl/gl_defer.h"

/**
 * Maximum number of milliseconds spent uploading textures at the beginning
 * of a frame. Note that non-uploaded textures will appear as pure white
 * until their content gets uploaded (you should precache them).
 */
#define FRAME_DEFERRED_UPLOAD_TIMEOUT 20

boolean drawGame = true; // If false the game viewport won't be rendered

using namespace de;

DENG2_PIMPL(LegacyWidget)
{
    Instance(Public *i) : Base(i)
    {}
};

LegacyWidget::LegacyWidget(String const &name)
    : GuiWidget(name), d(new Instance(this))
{}

void LegacyWidget::viewResized()
{
    if(isDisabled() || Sys_IsShuttingDown()) return;

    LOG_AS("LegacyWidget");
    LOG_DEBUG("View resized to ") << root().viewSize().asText();

    // Update viewports.
    R_SetViewGrid(0, 0);
    if(BusyMode_Active() || UI_IsActive() || !App_GameLoaded())
    {
        // Update for busy mode.
        R_UseViewPort(0);
    }
    R_LoadSystemFonts();
    if(UI_IsActive())
    {
        UI_UpdatePageLayout();
    }
}

void LegacyWidget::update()
{    
    if(isDisabled()) return;

    //LOG_DEBUG("Legacy update");

    DENG2_ASSERT(!BusyMode_Active());

    // We may be performing GL operations.
    Window_GLActivate(Window_Main());

    // Run at least one (fractional) tic.
    Loop_RunTics();

    // We may have received a Quit message from the windowing system
    // during events/tics processing.
    if(Sys_IsShuttingDown())
        return;

    GL_ProcessDeferredTasks(FRAME_DEFERRED_UPLOAD_TIMEOUT);

    // Request update of window contents.
    Window_Draw(Window_Main());

    // After the first frame, start timedemo.
    //DD_CheckTimeDemo();
}

void LegacyWidget::draw()
{
    if(renderWireframe || isDisabled())
    {
        // When rendering is wireframe mode, we must clear the screen
        // before rendering a frame.
        glClear(GL_COLOR_BUFFER_BIT);
    }

    if(isDisabled()) return;

    //LOG_DEBUG("Legacy draw");

    if(drawGame)
    {
        if(App_GameLoaded())
        {
            // Interpolate the world ready for drawing view(s) of it.
            if(theMap)
            {
                R_BeginWorldFrame();
            }
            R_RenderViewPorts();
        }
        else if(titleFinale == 0)
        {
            // Title finale is not playing. Lets do it manually.
            glMatrixMode(GL_PROJECTION);
            glPushMatrix();
            glLoadIdentity();
            glOrtho(0, SCREENWIDTH, SCREENHEIGHT, 0, -1, 1);

            R_RenderBlankView();

            glMatrixMode(GL_PROJECTION);
            glPopMatrix();
        }

        if(!(UI_IsActive() && UI_Alpha() >= 1.0))
        {
            UI2_Drawer();

            // Draw any full window game graphics.
            if(App_GameLoaded() && gx.DrawWindow)
                gx.DrawWindow(Window_Size(theWindow));
        }
    }

    if(Con_TransitionInProgress())
        Con_DrawTransition();

    if(drawGame)
    {
        // Debug information.
        Net_Drawer();
        S_Drawer();

        // Finish up any tasks that must be completed after view(s) have been drawn.
        R_EndWorldFrame();
    }

    if(UI_IsActive())
    {
        // Draw user interface.
        UI_Drawer();
    }

    // Draw console.
    Rend_Console();

    // End any open DGL sequence.
    DGL_End();
}

bool LegacyWidget::handleEvent(Event const &/*event*/)
{
    /// @todo Event processing should occur here, not during Loop_RunTics().
    return false;
}
