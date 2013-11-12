/** @file gamewidget.cpp
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

#include "de_platform.h" // must be included first

#include "ui/widgets/gamewidget.h"
#include "clientapp.h"
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
#include "edit_bias.h"
#include "world/map.h"
#include "network/net_main.h"
#include "render/r_main.h"
#include "render/rend_main.h"
#include "render/cameralensfx.h"
#include "audio/s_main.h"
#include "render/lightgrid.h"
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

//boolean drawGame = true; // If false the game viewport won't be rendered

using namespace de;

DENG2_PIMPL(GameWidget)
{
    Instance(Public *i) : Base(i) {}

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

        if(App_GameLoaded())
        {
            // Notify the world that a new render frame has begun.
            App_World().beginFrame(CPP_BOOL(R_NextViewer()));

            R_RenderViewPorts(ui::Player3DViewLayer);
            R_RenderViewPorts(ui::ViewBorderLayer);

            // End any open DGL sequence.
            DGL_End();

            // Notify the world that we've finished rendering the frame.
            App_World().endFrame();
        }

        // End any open DGL sequence.
        DGL_End();
    }

    void updateSize()
    {
        LOG_AS("GameWidget");
        LOG_TRACE("View resized to ") << self.rule().recti().size().asText();

        // Update viewports.
        R_SetViewGrid(0, 0);
        if(UI_IsActive() || !App_GameLoaded())
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
};

GameWidget::GameWidget(String const &name)
    : GuiWidget(name), d(new Instance(this))
{
    requestGeometry(false);
}

void GameWidget::glApplyViewport(int x, int y, int width, int height)
{
    GLState::top()
            .setNormalizedViewport(normalizedRect(Rectanglei(x, y, width, height)))
            .apply();
}

void GameWidget::viewResized()
{
    GuiWidget::viewResized();

    /*
    if(BusyMode_Active() || isDisabled() || Sys_IsShuttingDown() ||
       !ClientApp::windowSystem().hasMain())
    {
        return;
    }

    d->updateSize();*/
}

void GameWidget::update()
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

void GameWidget::drawContent()
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

    GLState::push();

    Rectanglei pos;
    if(hasChangedPlace(pos))
    {
        // Automatically update if the widget is resized.
        d->updateSize();
    }

    d->draw();

    GLState::considerNativeStateUndefined();
    GLState::pop();

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

bool GameWidget::handleEvent(Event const &event)
{
    /**
     * @todo Event processing should occur here, not during Loop_RunTics().
     * However, care must be taken to reproduce the vanilla behavior of
     * controls with regard to response times.
     *
     * @todo Input drivers need to support Unicode text; for now we have to
     * submit as Latin1.
     */

    if(event.type() == Event::MouseButton && !root().window().canvas().isMouseTrapped())
    {
        if(!root().window().hasSidebar())
        {
            // If the mouse is not trapped, we will just eat button clicks which
            // will prevent them from reaching the legacy input system.
            return true;
        }

        // If the sidebar is open, we must explicitly click on the GameWidget to
        // cause input to be trapped.
        switch(handleMouseClick(event))
        {
        case MouseClickFinished:
            // Click completed on the widget, trap the mouse.
            root().window().canvas().trapMouse();
            root().window().taskBar().close();
            root().setFocus(0); // Allow input to reach here.
            break;

        default:
            // Just ignore the event.
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
