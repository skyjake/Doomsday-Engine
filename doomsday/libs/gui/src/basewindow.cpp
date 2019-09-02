/** @file basewindow.cpp  Abstract base class for application windows.
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de/BaseWindow"
#include "de/WindowTransform"
#include "de/WindowSystem"
#include "de/BaseGuiApp"
#include "de/VRConfig"

#include <de/GLBuffer>
#include <de/GLState>
#include <de/LogBuffer>

namespace de {

DE_PIMPL(BaseWindow)
, DE_OBSERVES(GLWindow,         Init)
, DE_OBSERVES(KeyEventSource,   KeyEvent)
, DE_OBSERVES(MouseEventSource, MouseEvent)
//, DE_OBSERVES(MouseEventSource, MouseStateChange)
{
    WindowTransform defaultXf; ///< Used by default (doesn't apply any transformation).
    WindowTransform *xf;

    Impl(Public *i)
        : Base(i)
        , defaultXf(*i)
        , xf(&defaultXf)
    {
        self().audienceForInit() += this;

        // Listen to input.
        self().eventHandler().audienceForKeyEvent()         += this;
        self().eventHandler().audienceForMouseEvent()       += this;
//        self().eventHandler().audienceForMouseStateChange() += this;
    }

    void windowInit(GLWindow &) override
    {
        // The framework widgets expect basic alpha blending.
        GLState::current()
                .setBlend(true)
                .setBlendFunc(gfx::SrcAlpha, gfx::OneMinusSrcAlpha);
    }

    void keyEvent(KeyEvent const &ev) override
    {
        /// @todo Input drivers should observe the notification instead, input
        /// subsystem passes it to window system. -jk

        LOGDEV_INPUT_XVERBOSE("keyEvent ev:%i", ev.type());

        // Pass the event onto the window system.
//        if (!
        WindowSystem::get().processEvent(ev);
//                )
//        {
//            // Maybe the fallback handler has use for this.
//            self().handleFallbackEvent(ev);
//        }
    }

    void mouseEvent(MouseEvent const &event) override
    {
        MouseEvent ev = event;

        // Translate mouse coordinates for direct interaction.
        if (ev.type() == Event::MousePosition ||
            ev.type() == Event::MouseButton   ||
            ev.type() == Event::MouseWheel)
        {
            ev.setPos(xf->windowToLogicalCoords(event.pos()).toVec2i());
        }

//        if (!
        WindowSystem::get().processEvent(ev);
//        {
//            // Maybe the fallback handler has use for this.
//            self().handleFallbackEvent(ev);
//        }
    }

//    void mouseStateChanged(MouseEventSource::State state) override
//    {
//        self().grabInput(state == MouseEventSource::Trapped);
//    }
};

BaseWindow::BaseWindow(String const &id)
#if !defined (DE_MOBILE)
    : LIBAPPFW_BASEWINDOW_SUPER(id)
    , d(new Impl(this))
#else
    : d(new Impl(this))
#endif
{}

void BaseWindow::setTransform(WindowTransform &xf)
{
    d->xf = &xf;
}

void BaseWindow::useDefaultTransform()
{
    d->xf = &d->defaultXf;
}

WindowTransform &BaseWindow::transform()
{
    DE_ASSERT(d->xf != nullptr);
    return *d->xf;
}

#if 0
bool BaseWindow::prepareForDraw()
{
    if (isGLReady())
    {
#if !defined (DE_MOBILE)
        // Don't run the main loop until after the paint event has been dealt with.
        DE_GUI_APP->loop().pause();
#endif
        return true; // Go ahead.
    }
    return false;
}

void BaseWindow::requestDraw()
{
    update();

    if (!prepareForDraw())
    {
        // Not right now, please.
        return;
    }
}
#endif

void BaseWindow::draw()
{
    preDraw();
    d->xf->drawTransformed();
    postDraw();
}

void BaseWindow::preDraw()
{
    auto &vr = DE_BASE_GUI_APP->vr();

    // Initialize Oculus Rift if needed.
    if (vr.mode() == VRConfig::OculusRift)
    {
        if (isGLReady())
        {
            makeCurrent();
            vr.oculusRift().init();
            vr.oculusRift().beginFrame();
        }
    }
    else
    {
        makeCurrent();
        vr.oculusRift().deinit();
    }
}

void BaseWindow::postDraw()
{
    auto &vr = DE_BASE_GUI_APP->vr();
    if (vr.mode() == VRConfig::OculusRift)
    {
        vr.oculusRift().endFrame();
    }

#if !defined (DE_MOBILE)
    // The timer loop was paused when the frame was requested to be drawn.
    DE_GUI_APP->loop().resume();
#endif
}

#if defined (DE_MOBILE)

String BaseWindow::configName(String const &key) const
{
    return QString("window.main.%1").arg(key);
}

#endif

} // namespace de
