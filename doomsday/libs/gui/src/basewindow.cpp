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

#include "de/basewindow.h"

#include "de/baseguiapp.h"
#include "de/glbuffer.h"
#include "de/glstate.h"
#include "de/guirootwidget.h"
#include "de/vrconfig.h"
#include "de/windowsystem.h"
#include "de/windowtransform.h"

#include <de/logbuffer.h>

namespace de {

DE_PIMPL(BaseWindow)
, DE_OBSERVES(GLWindow,         Init)
, DE_OBSERVES(KeyEventSource,   KeyEvent)
, DE_OBSERVES(MouseEventSource, MouseEvent)
{
    // Mouse motion: collect excessive mouse move events into one.
    bool  mouseMoved = false;
    Vec2i latestMousePos;

    WindowTransform defaultXf; ///< Used by default (doesn't apply any transformation).
    WindowTransform *xf;

    Impl(Public *i)
        : Base(i)
        , defaultXf(*i)
        , xf(&defaultXf)
    {
        self().audienceForInit() += this;

        // Listen to input.
        self().eventHandler().audienceForKeyEvent() += this;
        self().eventHandler().audienceForMouseEvent() += this;
    }

    void windowInit(GLWindow &) override
    {
        // The framework widgets expect basic alpha blending.
        GLState::current()
                .setBlend(true)
                .setBlendFunc(gfx::SrcAlpha, gfx::OneMinusSrcAlpha);
    }

    void keyEvent(const KeyEvent &ev) override
    {
        LOGDEV_INPUT_XVERBOSE("keyEvent ev:%i", ev.type());
        self().root().processEvent(ev);
    }

    void mouseEvent(const MouseEvent &event) override
    {
        MouseEvent ev = event;

        // Translate mouse coordinates for direct interaction.
        if (ev.type() == Event::MousePosition ||
            ev.type() == Event::MouseButton   ||
            ev.type() == Event::MouseWheel)
        {
            ev.setPos(xf->windowToLogicalCoords(event.pos()).toVec2i());
        }

        /*
         * Mouse motion is filtered as it may be produced needlessly often with
         * high-frequency mice. Note that this does not affect relative mouse
         * events, just the absolute positions that interact with UI widgets.
         */
        if (ev.type() == Event::MousePosition)
        {
            if (ev.pos() != latestMousePos)
            {
                // This event will be emitted later, before widget tree update.
                latestMousePos = ev.pos();
                mouseMoved = true;
            }
            return;
        }

        self().root().processEvent(ev);
    }
};

BaseWindow::BaseWindow(const String &id)
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

void BaseWindow::processLatestMousePosition(bool evenIfUnchanged)
{
    if (d->mouseMoved || evenIfUnchanged)
    {
        d->mouseMoved = false;
        root().processEvent(MouseEvent(MouseEvent::Absolute, d->latestMousePos));
    }
}

Vec2i BaseWindow::latestMousePosition() const
{
    return d->latestMousePos;
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

void BaseWindow::rootUpdate()
{
    glActivate();
    processLatestMousePosition();
    root().update();
}

#if defined (DE_MOBILE)

String BaseWindow::configName(const String &key) const
{
    return QString("window.main.%1").arg(key);
}

#endif

} // namespace de
