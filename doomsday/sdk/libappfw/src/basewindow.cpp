/** @file basewindow.cpp  Abstract base class for application windows.
 *
 * @authors Copyright (c) 2014-2016 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <QElapsedTimer>
#include <QOpenGLTimerQuery>
#include <QVector>

namespace de {

DENG2_PIMPL(BaseWindow)
, DENG2_OBSERVES(Canvas,           GLInit)
, DENG2_OBSERVES(Canvas,           GLSwapped)
, DENG2_OBSERVES(KeyEventSource,   KeyEvent)
, DENG2_OBSERVES(MouseEventSource, MouseEvent)
{
    WindowTransform defaultXf; ///< Used by default (doesn't apply any transformation).
    WindowTransform *xf;
    QOpenGLTimerQuery timerQuery;
    bool timerQueryPending = false;
    QElapsedTimer gpuTimeRecordingStartedAt;
    QVector<TimeDelta> recordedGpuTimes;

    Impl(Public *i)
        : Base(i)
        , defaultXf(self)
        , xf(&defaultXf)
    {
        self.canvas().audienceForGLInit()     += this;
        self.canvas().audienceForGLSwapped()  += this;

        // Listen to input.
        self.canvas().audienceForKeyEvent()   += this;
        self.canvas().audienceForMouseEvent() += this;
    }

    void canvasGLInit(Canvas &)
    {
        // The framework widgets expect basic alpha blending.
        GLState::current()
                .setBlend(true)
                .setBlendFunc(gl::SrcAlpha, gl::OneMinusSrcAlpha);
    }

    void canvasGLSwapped(Canvas &)
    {
        // Measure how long it takes to render a frame on average.
        if (GLInfo::extensions().EXT_timer_query &&
            timerQueryPending &&
            timerQuery.isResultAvailable())
        {
            timerQueryPending = false;
            recordedGpuTimes.append(double(timerQuery.waitForResult()) / 1.0e9);

            if (!gpuTimeRecordingStartedAt.isValid())
            {
                gpuTimeRecordingStartedAt.start();
            }

            // There are minor time variations rendering the same frame, so average over
            // a second to find out a reasonable value.
            if (gpuTimeRecordingStartedAt.elapsed() > 1000)
            {
                TimeDelta average = 0;
                for (auto dt : recordedGpuTimes) average += dt;
                average = average / recordedGpuTimes.size();
                recordedGpuTimes.clear();

                qDebug() << "[OpenGL average frame timed]" << average.asMicroSeconds() << "µs";
                qDebug() << "[OpenGL draw count]" << GLBuffer::drawCount();

                gpuTimeRecordingStartedAt.restart();
            }
        }
    }

    void keyEvent(KeyEvent const &ev)
    {
        /// @todo Input drivers should observe the notification instead, input
        /// subsystem passes it to window system. -jk

        // Pass the event onto the window system.
        if (!WindowSystem::get().processEvent(ev))
        {
            // Maybe the fallback handler has use for this.
            self.handleFallbackEvent(ev);
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
            self.handleFallbackEvent(ev);
        }
    }

    bool timerQueryReady() const
    {
        if (!GLInfo::extensions().EXT_timer_query) return false;
        return timerQuery.isCreated() && !timerQueryPending;
    }
};

BaseWindow::BaseWindow(String const &id)
    : PersistentCanvasWindow(id)
    , d(new Impl(this))
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
    GLBuffer::resetDrawCount();

    // Don't run the main loop until after the paint event has been dealt with.
    DENG2_GUI_APP->loop().pause();
    return true; // Go ahead.
}

void BaseWindow::requestDraw()
{
    if (!prepareForDraw())
    {
        // Not right now, please.
        return;
    }

    // Initialize Oculus Rift if needed.
    auto &vr = DENG2_BASE_GUI_APP->vr();
    if (vr.mode() == VRConfig::OculusRift)
    {
        if (canvas().isGLReady())
        {
            canvas().makeCurrent();
            vr.oculusRift().init();
        }
    }
    else
    {
        canvas().makeCurrent();
        vr.oculusRift().deinit();
    }

    canvas().update();
}

void BaseWindow::draw()
{
    preDraw();
    d->xf->drawTransformed();
    postDraw();

    PersistentCanvasWindow::draw();
}

void BaseWindow::preDraw()
{
    if (GLInfo::extensions().EXT_timer_query)
    {
        if (!d->timerQuery.isCreated())
        {
            if (!d->timerQuery.create())
            {
                LOG_GL_ERROR("Failed to create timer query object");
            }
        }

        if (d->timerQueryReady())
        {
            d->timerQuery.begin();
        }
    }

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

    if (d->timerQueryReady())
    {
        d->timerQuery.end();
        d->timerQueryPending = true;
    }

    // The timer loop was paused when the frame was requested to be drawn.
    DENG2_GUI_APP->loop().resume();
}

} // namespace de
