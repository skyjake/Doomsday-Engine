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
#include "ui/widgets/legacywidget.h"
#include "ui/dd_input.h"
#include "ui/ui_main.h"
#include "ui/ui2_main.h"
#include "ui/sys_input.h"
#include "ui/busyvisual.h"
#include "ui/windowsystem.h"
#include "ui/widgets/taskbarwidget.h"
#include "dd_def.h"
#include "dd_main.h"
#include "dd_loop.h"
#include "sys_system.h"
#ifdef __CLIENT__
#  include "edit_bias.h"
#endif
#include "world/map.h"
#include "network/net_main.h"
#include "render/r_main.h"
#include "render/rend_list.h"
//#include "render/rend_console.h"
#include "audio/s_main.h"
#include "gl/gl_main.h"
#include "gl/sys_opengl.h"
#include "gl/gl_defer.h"

#include <de/GLState>

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

    void draw()
    {
        bool cannotDraw = (self.isDisabled() || !GL_IsFullyInited());

        if(renderWireframe || cannotDraw)
        {
            // When rendering is wireframe mode, we must clear the screen
            // before rendering a frame.
            glClear(GL_COLOR_BUFFER_BIT);
        }

        if(cannotDraw) return;

        if(drawGame)
        {
            if(App_GameLoaded())
            {
                // Notify the world that a new render frame has begun.
                App_World().beginFrame(CPP_BOOL(R_NextViewer()));

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
                {
                    Size2Raw dimensions(DENG_WINDOW->width(), DENG_WINDOW->height());
                    gx.DrawWindow(&dimensions);
                }
            }
        }

        if(Con_TransitionInProgress())
            Con_DrawTransition();

        if(drawGame)
        {
            // Draw the widgets of the Shadow Bias Editor (if active).
            SBE_DrawGui();

            /*
             * Draw debug information.
             */
            if(App_World().hasMap() && App_World().map().hasLightGrid())
            {
                App_World().map().lightGrid().drawDebugVisual();
            }
            Net_Drawer();
            S_Drawer();

            // Notify the world that we've finished rendering the frame.
            App_World().endFrame();
        }

        if(UI_IsActive())
        {
            // Draw user interface.
            UI_Drawer();
        }

        // Draw console.
        //Rend_Console();

        // End any open DGL sequence.
        DGL_End();
    }
};

LegacyWidget::LegacyWidget(String const &name)
    : GuiWidget(name), d(new Instance(this))
{}

void LegacyWidget::viewResized()
{
    if(BusyMode_Active() || isDisabled() || Sys_IsShuttingDown()) return;

    LOG_AS("LegacyWidget");
    LOG_TRACE("View resized to ") << root().viewSize().asText();

    // Update viewports.
    R_SetViewGrid(0, 0);
    if(/*BusyMode_Active() ||*/ UI_IsActive() || !App_GameLoaded())
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
    GuiWidget::update();

    if(isDisabled()) return;

    //LOG_DEBUG("Legacy update");

    DENG2_ASSERT(!BusyMode_Active());

    // We may be performing GL operations.
    ClientWindow::main().glActivate();

    // Run at least one (fractional) tic.
    Loop_RunTics();

    // We may have received a Quit message from the windowing system
    // during events/tics processing.
    if(Sys_IsShuttingDown())
        return;

    GL_ProcessDeferredTasks(FRAME_DEFERRED_UPLOAD_TIMEOUT);

    // Request update of window contents.
    root().window().draw();

    // After the first frame, start timedemo.
    //DD_CheckTimeDemo();
}

void LegacyWidget::drawContent()
{
    if(isDisabled() || !GL_IsFullyInited())
        return;

#if 0
    glMatrixMode(GL_TEXTURE);
    glPushMatrix();
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();

    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);

    GL_Init2DState();
#endif

    d->draw();

#if 0
    glPopClientAttrib();
    glPopAttrib();

    glMatrixMode(GL_TEXTURE);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
#endif
}

bool LegacyWidget::handleEvent(Event const &event)
{
    /**
     * @todo Event processing should occur here, not during Loop_RunTics().
     * However, care must be taken to reproduce the vanilla behavior of
     * controls with regard to response times.
     *
     * @todo Input drivers need to support Unicode text; for now we have to
     * submit as Latin1.
     */

    // Clicking on the game view will trap the mouse automatically.
    Canvas &canvas = root().window().canvas();
    if(!canvas.isMouseTrapped() && event.type() == Event::MouseButton &&
       App_GameLoaded())
    {
        MouseEvent const &mouse = event.as<MouseEvent>();
        if(mouse.state() == MouseEvent::Pressed && hitTest(mouse.pos()))
        {
            if(root().focus())
            {
                // First click will remove UI focus, allowing LegacyWidget
                // to receive events.
                root().setFocus(0);
                return true;
            }

            canvas.trapMouse();
            root().window().taskBar().close();
            return true;
        }
    }

    if(event.type() == Event::KeyPress ||
       event.type() == Event::KeyRepeat ||
       event.type() == Event::KeyRelease)
    {
        KeyEvent const &ev = event.as<KeyEvent>();
        Keyboard_Submit(ev.state() == KeyEvent::Pressed? IKE_DOWN :
                        ev.state() == KeyEvent::Repeat?  IKE_REPEAT :
                                                         IKE_UP,
                        ev.ddKey(), ev.nativeCode(), ev.text().toLatin1());
    }

    return false;
}
