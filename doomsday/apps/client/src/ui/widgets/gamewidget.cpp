/** @file gamewidget.cpp
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2014-2015 Daniel Swanson <danij@dengine.net>
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
#include "api_render.h"

#include "ui/widgets/gamewidget.h"
#include "clientapp.h"
#include "ui/ddevent.h"
#include "ui/ui_main.h"
#include "ui/sys_input.h"
#include "ui/busyvisual.h"
#include "ui/inputsystem.h"
#include "ui/viewcompositor.h"
#include "ui/widgets/taskbarwidget.h"
#include "ui/widgets/busywidget.h"
#include "dd_def.h"
#include "dd_main.h"
#include "dd_loop.h"
#include "sys_system.h"
//#include "ui/editors/edit_bias.h"
#include "world/map.h"
#include "world/p_players.h"
#include "network/net_main.h"
#include "client/cl_def.h" // clientPaused
#include "render/r_main.h"
#include "render/rend_main.h"
#include "render/cameralensfx.h"
//#include "render/lightgrid.h"
#include "gl/gl_main.h"
#include "gl/sys_opengl.h"
#include "gl/gl_defer.h"

#include <doomsday/console/exec.h>
#include <de/filesystem.h>
#include <de/glstate.h>
#include <de/gltextureframebuffer.h>
#include <de/logbuffer.h>
#include <de/vrconfig.h>

/**
 * Maximum number of milliseconds spent uploading textures at the beginning
 * of a frame. Note that non-uploaded textures will appear as pure white
 * until their content gets uploaded (you should precache them).
 */
#define FRAME_DEFERRED_UPLOAD_TIMEOUT 20

using namespace de;

DE_PIMPL(GameWidget)
{
    bool          needFrames   = true; // Rendering a new frame is necessary.
    VRConfig::Eye lastFrameEye = VRConfig::NeitherEye;

    Impl(Public *i) : Base(i) {}

    /**
     * Render the 3D game view of each local player. The results are stored in each
     * players' ViewCompositor as a texture. This is a slow operation and should only
     * be done once after each time game tics have been run.
     */
    void renderGameViews()
    {
        lastFrameEye = ClientApp::vr().currentEye();

        ClientApp::forLocalPlayers([] (ClientPlayer &player)
        {
            player.viewCompositor().renderGameView([] (int playerNum) {
                R_RenderViewPort(playerNum);
            });
            return LoopContinue;
        });
    }

    void renderPlayerViewToFramebuffer(int playerNum, GLFramebuffer &dest)
    {
        GLState::push()
                .setTarget(dest)
                .setViewport(Rectangleui::fromSize(dest.size()));

        dest.clear(GLFramebuffer::ColorDepthStencil);

        // Rendering is done by the caller-provided callback.
        R_RenderViewPort(playerNum);

        GLState::pop();
    }

    /**
     * Draw the game widget's contents by compositing the various layers: game view,
     * border, HUD, finale, intermission, and engine/debug overlays. This is generally
     * a quick operation and can be done multiple times per window paint.
     */
    void drawComposited()
    {
        int numLocal = 0;
        ClientApp::forLocalPlayers([&numLocal](ClientPlayer &player)
        {
            player.viewCompositor().drawCompositedLayers();
            ++numLocal;
            return LoopContinue;
        });

        // Nobody is playing right now?
        if (numLocal == 0)
        {
            ClientApp::player(0).viewCompositor().drawCompositedLayers();
        }
    }

    void draw()
    {
        // If the eye changes, we will need to redraw the view.
        if (ClientApp::vr().currentEye() != lastFrameEye)
        {
            needFrames = true;
        }

        if (needFrames)
        {
            using namespace world;

            // Notify the world that a new render frame has begun.
            ClientApp::world().notifyFrameState(World::FrameBegins);

            // Each players' view is rendered into an FBO first. What is seen on screen
            // is then composited using the player view as a texture with additional layers
            // and effects.
            renderGameViews();

            // End any open DGL sequence.
            DGL_End();
            DGL_Flush();

            // Notify the world that we've finished rendering the frame.
            ClientApp::world().notifyFrameState(World::FrameEnds);

            needFrames = false;
        }

        drawComposited();
    }

    void updateSize()
    {
        LOG_AS("GameWidget");
        LOG_GL_XVERBOSE("View resized to ", self().rule().recti().size().asText());

        // Update viewports.
        R_SetViewGrid(0, 0);
        /*if (!App_GameLoaded())
        {
            // Update for busy mode.
            R_UseViewPort(0);
        }*/
        //UI_LoadFonts();
    }
};

GameWidget::GameWidget(const String &name)
    : GuiWidget(name), d(new Impl(this))
{
    requestGeometry(false);
}

void GameWidget::pause()
{
    if (App_GameLoaded() && !clientPaused)
    {
        Con_Execute(CMDS_DDAY, "pause", true, false);
    }
}

void GameWidget::drawComposited()
{
    d->drawComposited();
}

void GameWidget::renderCubeMap(uint size, const String &outputImagePath)
{
    using namespace world;

    const int player = consolePlayer;
    Vec2ui fbSize(size, size);

    LOG_GL_MSG("Rendering %ix%i cube map...") << 6 * fbSize.x << fbSize.y;

    // Prevent the angleclipper from clipping anything.
    const int old_devNoCulling = devNoCulling;
    devNoCulling = 1;

    // Make the player temporarily a plain camera to hide weapons etc.
    ClientPlayer &plr = ClientApp::player(player);
    const auto oldPlrFlags = plr.publicData().flags;
    plr.publicData().flags |= DDPF_CAMERA;

    // Notify the world that a new render frame has begun.
    World::get().notifyFrameState(World::FrameBegins);

    Image composited(Image::Size(6 * size, size), Image::RGB_888);

    const int baseYaw = 180;

    GLTextureFramebuffer destFb(Image::RGB_888, fbSize, 1);
    destFb.glInit();
    for (int i = 0; i < 6; ++i)
    {
        if (i < 4)
        {
            Rend_SetFixedView(player, baseYaw + 90 + i * -90, 0, 90, fbSize);
        }
        else
        {
            Rend_SetFixedView(player, baseYaw, i == 4? -90 : 90, 90, fbSize);
        }
        d->renderPlayerViewToFramebuffer(player, destFb);
        composited.draw(i * size, 0, destFb.toImage());

        // IssueID #2401: Somewhere the GL state is messed up, only the first view would
        // be visible. A proper fix would be to look through the GL state changes during
        // rendering.
        GLState::considerNativeStateUndefined();
    }
    destFb.glDeinit();

    World::get().notifyFrameState(World::FrameEnds);

    // Write the composited image to a file.
    {
        const Block buf = composited.serialize(outputImagePath.fileNameExtension().toString());

        // Choose a unique name.
        int    counter    = 0;
        String uniquePath = outputImagePath;
        while (FS::tryLocate<File const>(uniquePath))
        {
            uniquePath = outputImagePath.fileNameAndPathWithoutExtension() +
                    Stringf("-%03i", counter++) +
                    outputImagePath.fileNameExtension();
        }

        File &outFile = FS::get().root().replaceFile(uniquePath);
        outFile << buf;
        outFile.flush();

        LOG_GL_MSG("Cube map saved to \"%s\"") << outFile.correspondingNativePath();
    }

    // Cleanup.
    Rend_UnsetFixedView();
    devNoCulling = old_devNoCulling;
    plr.publicData().flags = oldPlrFlags;
}

void GameWidget::viewResized()
{
    GuiWidget::viewResized();

    if (!BusyMode_Active() && !isDisabled() && !Sys_IsShuttingDown() &&
        ClientWindow::mainExists())
    {
        d->updateSize();
    }
}

void GameWidget::update()
{
    GuiWidget::update();

    if (isDisabled() || BusyMode_Active()) return;

    // We may be performing GL operations.
    ClientWindow::main().glActivate();

    // Run at least one (fractional) tic.
    Loop_RunTics();

    // Time has progressed, so game views need rendering.
    d->needFrames = true;

    // We may have received a Quit message from the windowing system
    // during events/tics processing.
    if (Sys_IsShuttingDown()) return;

    GL_ProcessDeferredTasks(FRAME_DEFERRED_UPLOAD_TIMEOUT);

    // Release the busy transition frame now when we can be sure that busy mode
    // is over / didn't start at all.
    if (!Con_TransitionInProgress())
    {
        ClientWindow::main().busy().releaseTransitionFrame();
    }
}

void GameWidget::drawContent()
{
    if (isDisabled() || !GL_IsFullyInited() || !App_GameLoaded())
        return;

    root().painter().flush();
    GLState::push();

    Rectanglei pos;
    if (hasChangedPlace(pos))
    {
        // Automatically update if the widget is resized.
        d->updateSize();
    }

    d->draw();

    GLState::considerNativeStateUndefined();
    GLState::pop();
}

bool GameWidget::handleEvent(const Event &event)
{
    /**
     * @todo Event processing should occur here, not during Loop_RunTics().
     * However, care must be taken to reproduce the vanilla behavior of
     * controls with regard to response times.
     *
     * @todo Input drivers need to support Unicode text; for now we have to
     * submit as Latin1.
     */

    ClientWindow &window = root().window().as<ClientWindow>();

    if (event.type() == Event::MouseButton && !root().window().eventHandler().isMouseTrapped() &&
        rule().recti().contains(event.as<MouseEvent>().pos()))
    {
        if (!window.hasSidebar() && !window.isGameMinimized())
        {
            // If the mouse is not trapped, we will just eat button clicks which
            // will prevent them from reaching the legacy input system.
            return true;
        }

        // If the sidebar is open, we must explicitly click on the GameWidget to
        // cause input to be trapped.
        switch (handleMouseClick(event))
        {
        case MouseClickFinished:
            // Click completed on the widget, trap the mouse.
            window.eventHandler().trapMouse();
            window.taskBar().close();
            root().setFocus(this);         // Allow input to reach here.
//            root().clearFocusStack();   // Ensure no popups steal focus away.
            break;

        default:
            // Just ignore the event.
            return true;
        }
    }

//    if (hasFocus())
    {
        switch (event.type())
        {
        case Event::KeyPress:
        case Event::KeyRepeat:
        case Event::KeyRelease:
            InputSystem::get().postKeyboardEvent(event.as<KeyEvent>());
            return true;

        case Event::MouseButton:
        case Event::MouseMotion:
        case Event::MouseWheel:
            InputSystem::get().postMouseEvent(event.as<MouseEvent>());
            return true;

//            const auto &ev = event.as<KeyEvent>();
//            Keyboard_Submit(ev.state() == KeyEvent::Pressed? IKE_DOWN :
//                            ev.state() == KeyEvent::Repeat?  IKE_REPEAT :
//                                                             IKE_UP,
//                            ev.ddKey(), ev.scancode(), ev.text());
        }
    }

    return false;
}

void GameWidget::glDeinit()
{
    GuiWidget::glDeinit();

    //d->glDeinit();
}

D_CMD(CubeShot)
{
    DE_UNUSED(src, argc);

    int size = String(argv[1]).toInt();
    if (size < 8) return false;
    ClientWindow::main().game().renderCubeMap(size, "/home/cubeshot.png");
    return true;
}
