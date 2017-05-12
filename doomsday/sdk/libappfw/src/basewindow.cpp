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

#include <QVector>

namespace de {

DENG2_PIMPL(BaseWindow)
, DENG2_OBSERVES(GLWindow,         Init)
, DENG2_OBSERVES(KeyEventSource,   KeyEvent)
, DENG2_OBSERVES(MouseEventSource, MouseEvent)
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
        self().eventHandler().audienceForKeyEvent()   += this;
        self().eventHandler().audienceForMouseEvent() += this;
    }

    void windowInit(GLWindow &)
    {
        // The framework widgets expect basic alpha blending.
        GLState::current()
                .setBlend(true)
                .setBlendFunc(gl::SrcAlpha, gl::OneMinusSrcAlpha);
    }

    void keyEvent(KeyEvent const &ev)
    {
        /// @todo Input drivers should observe the notification instead, input
        /// subsystem passes it to window system. -jk

        // Pass the event onto the window system.
        if (!WindowSystem::get().processEvent(ev))
        {
            // Maybe the fallback handler has use for this.
            self().handleFallbackEvent(ev);
        }
    }

    void mouseEvent(MouseEvent const &event)
    {
        MouseEvent ev = event;

        // Translate mouse coordinates for direct interaction.
        if (ev.type() == Event::MousePosition ||
            ev.type() == Event::MouseButton   ||
            ev.type() == Event::MouseWheel)
        {
            ev.setPos(xf->windowToLogicalCoords(event.pos()).toVector2i());
        }

        if (!WindowSystem::get().processEvent(ev))
        {
            // Maybe the fallback handler has use for this.
            self().handleFallbackEvent(ev);
        }
    }
};

BaseWindow::BaseWindow(String const &id)
#if !defined (DENG_MOBILE)
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
    DENG2_ASSERT(d->xf != 0);
    return *d->xf;
}

bool BaseWindow::prepareForDraw()
{
    if (isGLReady())
    {
        // Don't run the main loop until after the paint event has been dealt with.
        DENG2_GUI_APP->loop().pause();
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

    // Initialize Oculus Rift if needed.
    auto &vr = DENG2_BASE_GUI_APP->vr();
    if (vr.mode() == VRConfig::OculusRift)
    {
        if (isGLReady())
        {
            makeCurrent();
            vr.oculusRift().init();
        }
    }
    else
    {
        makeCurrent();
        vr.oculusRift().deinit();
    }
}

void BaseWindow::draw()
{
    preDraw();
    d->xf->drawTransformed();
    postDraw();
}

void BaseWindow::preDraw()
{
    auto &vr = DENG2_BASE_GUI_APP->vr();
    if (vr.mode() == VRConfig::OculusRift)
    {
        vr.oculusRift().beginFrame();
    }
}

void BaseWindow::postDraw()
{
    auto &vr = DENG2_BASE_GUI_APP->vr();
    if (vr.mode() == VRConfig::OculusRift)
    {
        vr.oculusRift().endFrame();
    }

    // The timer loop was paused when the frame was requested to be drawn.
    DENG2_GUI_APP->loop().resume();
}
    
#if defined (DENG_MOBILE)

String BaseWindow::configName(String const &key) const
{
    return QString("window.main.%1").arg(key);
}

#endif

} // namespace de
