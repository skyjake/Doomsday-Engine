/** @file canvas.cpp  OpenGL drawing surface (QWidget).
 *
 * @authors Copyright (c) 2012-2016 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/Canvas"
#include "de/CanvasWindow"
#include "de/GLState"
#include "de/GLTexture"
#include "de/graphics/opengl.h"

#include <de/App>
#include <de/Log>
#include <de/Loop>
#include <de/Drawable>
#include <de/GLInfo>
#include <de/GLState>
#include <de/GLTextureFramebuffer>

#include <QApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QShowEvent>
#include <QResizeEvent>
#include <QPaintEvent>
#include <QImage>
#include <QTimer>
#include <QTime>
#include <QDebug>
#include <QOpenGLTimerQuery>
#include <QElapsedTimer>

namespace de {

DENG2_PIMPL(Canvas)
{
    LoopCallback mainCall;

    GLTextureFramebuffer backing;

    CanvasWindow *parent;
    bool readyPending = false;
    bool readyNotified = false;
    Size currentSize;
    Size pendingSize;
    bool mouseGrabbed = false;
    QPoint prevMousePos;
    QTime prevWheelAt;
    QPoint wheelAngleAccum;
    int wheelDir[2];
#ifdef WIN32
    bool altIsDown = false;
#endif

    QOpenGLTimerQuery *timerQuery = nullptr;
    bool timerQueryPending = false;
    QElapsedTimer gpuTimeRecordingStartedAt;
    QVector<TimeDelta> recordedGpuTimes;

    Impl(Public *i, CanvasWindow *parentWindow)
        : Base(i)
        , parent(parentWindow)
    {
        wheelDir[0] = wheelDir[1] = 0;
    }

    ~Impl()
    {
        self.makeCurrent();
        glDeinit();
        self.doneCurrent();
    }

    void grabMouse()
    {
        if (!self.isVisible()) return;

        if (!mouseGrabbed)
        {
            LOG_INPUT_VERBOSE("Grabbing mouse") << mouseGrabbed;

            mouseGrabbed = true;

            DENG2_FOR_PUBLIC_AUDIENCE2(MouseStateChange, i)
            {
                i->mouseStateChanged(Trapped);
            }
        }
    }

    void ungrabMouse()
    {
        if (!self.isVisible()) return;

        if (mouseGrabbed)
        {
            LOG_INPUT_VERBOSE("Ungrabbing mouse");

            // Tell the mouse driver that the mouse is untrapped.
            mouseGrabbed = false;

            DENG2_FOR_PUBLIC_AUDIENCE2(MouseStateChange, i)
            {
                i->mouseStateChanged(Untrapped);
            }
        }
    }

    static int nativeCode(QKeyEvent const *ev)
    {
#if defined(UNIX) && !defined(MACOSX)
        return ev->nativeScanCode();
#else
        return ev->nativeVirtualKey();
#endif
    }

    void handleKeyEvent(QKeyEvent *ev)
    {
        //LOG_AS("Canvas");

        ev->accept();
        //if (ev->isAutoRepeat()) return; // Ignore repeats, we do our own.

        /*
        qDebug() << "Canvas: key press" << ev->key() << QString("0x%1").arg(ev->key(), 0, 16)
                 << "text:" << ev->text()
                 << "native:" << ev->nativeVirtualKey()
                 << "scancode:" << ev->nativeScanCode();
        */

#ifdef WIN32
        // We must track the state of the alt key ourselves as the OS grabs the up event...
        if (ev->key() == Qt::Key_Alt)
        {
            if (ev->type() == QEvent::KeyPress)
            {
                if (altIsDown) return; // Ignore repeat down events(!)?
                altIsDown = true;
            }
            else if (ev->type() == QEvent::KeyRelease)
            {
                if (!altIsDown)
                {
                    LOG_DEBUG("Ignoring repeat alt up.");
                    return; // Ignore repeat up events.
                }
                altIsDown = false;
                //LOG_DEBUG("Alt is up.");
            }
        }
#endif

        DENG2_FOR_PUBLIC_AUDIENCE2(KeyEvent, i)
        {
            i->keyEvent(KeyEvent(ev->isAutoRepeat()?             KeyEvent::Repeat :
                                 ev->type() == QEvent::KeyPress? KeyEvent::Pressed :
                                                                 KeyEvent::Released,
                                 ev->key(),
                                 KeyEvent::ddKeyFromQt(ev->key(), ev->nativeVirtualKey(), ev->nativeScanCode()),
                                 nativeCode(ev),
                                 ev->text(),
                                 (ev->modifiers().testFlag(Qt::ShiftModifier)?   KeyEvent::Shift   : KeyEvent::NoModifiers) |
                                 (ev->modifiers().testFlag(Qt::ControlModifier)? KeyEvent::Control : KeyEvent::NoModifiers) |
                                 (ev->modifiers().testFlag(Qt::AltModifier)?     KeyEvent::Alt     : KeyEvent::NoModifiers) |
                                 (ev->modifiers().testFlag(Qt::MetaModifier)?    KeyEvent::Meta    : KeyEvent::NoModifiers)));
        }
    }

    void reconfigureFramebuffer()
    {
        backing.setColorFormat(Image::RGB_888);
        backing.resize(currentSize);
    }

    void glInit()
    {
        DENG2_ASSERT(parent != 0);
        GLInfo::glInit();
        backing.glInit();
    }

    void glDeinit()
    {
        backing.glDeinit();
        GLInfo::glDeinit();
    }

    void notifyReady()
    {
        if (readyNotified) return;

        readyPending = false;

        self.makeCurrent();

        DENG2_ASSERT(QOpenGLContext::currentContext() != nullptr);

        reconfigureFramebuffer();

        // Print some information.
        QSurfaceFormat const fmt = self.format();

        LOG_GL_NOTE("OpenGL %i.%i supported%s")
                << fmt.majorVersion() << fmt.minorVersion()
                << (fmt.majorVersion() > 2?
                        (fmt.profile() == QSurfaceFormat::CompatibilityProfile? " (Compatibility)"
                                                                              : " (Core)") : "");

        // Everybody can perform GL init now.
        DENG2_FOR_PUBLIC_AUDIENCE2(GLInit, i) i->canvasGLInit(self);

        readyNotified = true;

        //update(); // Try again next frame.
        self.doneCurrent();

        // Now we can paint.
        mainCall.enqueue([this] () { self.update(); });
    }

    bool timerQueryReady() const
    {
        if (!GLInfo::extensions().EXT_timer_query) return false;
        return timerQuery && !timerQueryPending;
    }

    void checkTimerQueryResult()
    {
        // Measure how long it takes to render a frame on average.
        if (GLInfo::extensions().EXT_timer_query &&
            timerQueryPending &&
            timerQuery->isResultAvailable())
        {
            timerQueryPending = false;
            recordedGpuTimes.append(double(timerQuery->waitForResult()) / 1.0e9);

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

    template <typename QtEventType>
    Vector2i translatePosition(QtEventType const *ev) const
    {
        return Vector2i(ev->pos().x(), ev->pos().y()) * self.devicePixelRatio();
    }

    DENG2_PIMPL_AUDIENCE(GLInit)
    DENG2_PIMPL_AUDIENCE(GLResize)
    DENG2_PIMPL_AUDIENCE(GLSwapped)
    DENG2_PIMPL_AUDIENCE(FocusChange)
};

DENG2_AUDIENCE_METHOD(Canvas, GLInit)
DENG2_AUDIENCE_METHOD(Canvas, GLResize)
DENG2_AUDIENCE_METHOD(Canvas, GLSwapped)
DENG2_AUDIENCE_METHOD(Canvas, FocusChange)

Canvas::Canvas(CanvasWindow *parent)
    : QOpenGLWidget(parent)
    , d(new Impl(this, parent))
{
    LOG_AS("Canvas");

    connect(this, SIGNAL(frameSwapped()), this, SLOT(frameWasSwapped()));

    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}

QImage Canvas::grabImage(QSize const &outputSize)
{
    return grabImage(QRect(QPoint(0, 0), rect().size() * qApp->devicePixelRatio()), outputSize);
}

QImage Canvas::grabImage(QRect const &area, QSize const &outputSize)
{
    // We will be grabbing the visible, latest complete frame.
    //LIBGUI_GL.glReadBuffer(GL_FRONT);
    QImage grabbed = grabFramebuffer(); // no alpha
    if (area.size() != grabbed.size())
    {
        // Just take a portion of the full image.
        grabbed = grabbed.copy(area);
    }
    //LIBGUI_GL.glReadBuffer(GL_BACK);
    if (outputSize.isValid())
    {
        grabbed = grabbed.scaled(outputSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    return grabbed;
}

Canvas::Size Canvas::size() const
{
    return d->currentSize;
}

void Canvas::trapMouse(bool trap)
{
    if (trap)
    {
        d->grabMouse();
    }
    else
    {
        d->ungrabMouse();
    }
}

bool Canvas::isMouseTrapped() const
{
    return d->mouseGrabbed;
}

bool Canvas::isGLReady() const
{
    return d->readyNotified;
}

GLTextureFramebuffer &Canvas::framebuffer() const
{
    return d->backing;
}

void Canvas::initializeGL()
{
    LOG_AS("Canvas");
    LOGDEV_GL_NOTE("Initializing OpenGL window");

    d->glInit();
}

void Canvas::resizeGL(int w, int h)
{
    d->pendingSize = Size(max(0, w), max(0, h)) * qApp->devicePixelRatio();

    // Only react if this is actually a resize.
    if (d->currentSize != d->pendingSize)
    {
        d->currentSize = d->pendingSize;

        if (d->readyNotified)
        {
            makeCurrent();
            d->reconfigureFramebuffer();
        }

        DENG2_FOR_AUDIENCE2(GLResize, i) i->canvasGLResized(*this);
    }
}

void Canvas::frameWasSwapped()
{
    makeCurrent();
    DENG2_FOR_AUDIENCE2(GLSwapped, i) i->canvasGLSwapped(*this);
    doneCurrent();
}

void Canvas::paintGL()
{
    if (!d->parent) return;

    GLFramebuffer::setDefaultFramebuffer(defaultFramebufferObject());

    // Do not proceed with painting until after the application has completed
    // GL initialization.
    if (!d->readyNotified)
    {
        if (!d->readyPending)
        {
            d->readyPending = true;
            d->mainCall.enqueue([this] () { d->notifyReady(); });
        }
        return;
    }

    DENG2_ASSERT(QOpenGLContext::currentContext() != nullptr);

    if (GLInfo::extensions().EXT_timer_query)
    {
        d->checkTimerQueryResult();

        if (!d->timerQuery)
        {
            d->timerQuery = new QOpenGLTimerQuery(this);
            if (!d->timerQuery->create())
            {
                LOG_GL_ERROR("Failed to create timer query object");
            }
        }

        if (d->timerQueryReady())
        {
            d->timerQuery->begin();
        }
    }

    GLBuffer::resetDrawCount();

    LIBGUI_ASSERT_GL_OK();

    // Make sure any changes to the state stack are in effect.
    GLState::current().apply();
    GLState::current().target().glBind();

    // Window is responsible for drawing.
    d->parent->draw();

    LIBGUI_ASSERT_GL_OK();

    d->backing.blit();

    if (d->timerQueryReady())
    {
        d->timerQuery->end();
        d->timerQueryPending = true;
    }
}

void Canvas::focusInEvent(QFocusEvent*)
{
    LOG_AS("Canvas");
    LOG_INPUT_VERBOSE("Gained focus");

    DENG2_FOR_AUDIENCE2(FocusChange, i) i->canvasFocusChanged(*this, true);
}

void Canvas::focusOutEvent(QFocusEvent*)
{
    LOG_AS("Canvas");
    LOG_INPUT_VERBOSE("Lost focus");

    // Automatically ungrab the mouse if focus is lost.
    d->ungrabMouse();

    DENG2_FOR_AUDIENCE2(FocusChange, i) i->canvasFocusChanged(*this, false);
}

void Canvas::keyPressEvent(QKeyEvent *ev)
{
    d->handleKeyEvent(ev);
}

void Canvas::keyReleaseEvent(QKeyEvent *ev)
{
    d->handleKeyEvent(ev);
}

static MouseEvent::Button translateButton(Qt::MouseButton btn)
{
    if (btn == Qt::LeftButton)   return MouseEvent::Left;
    if (btn == Qt::MiddleButton) return MouseEvent::Middle;
    if (btn == Qt::RightButton)  return MouseEvent::Right;
    if (btn == Qt::XButton1)     return MouseEvent::XButton1;
    if (btn == Qt::XButton2)     return MouseEvent::XButton2;

    return MouseEvent::Unknown;
}

void Canvas::mousePressEvent(QMouseEvent *ev)
{
    ev->accept();

    DENG2_FOR_AUDIENCE2(MouseEvent, i)
    {
        i->mouseEvent(MouseEvent(translateButton(ev->button()), MouseEvent::Pressed,
                                 d->translatePosition(ev)));
    }
}

void Canvas::mouseReleaseEvent(QMouseEvent* ev)
{
    ev->accept();

    DENG2_FOR_AUDIENCE2(MouseEvent, i)
    {
        i->mouseEvent(MouseEvent(translateButton(ev->button()), MouseEvent::Released,
                                 d->translatePosition(ev)));
    }
}

void Canvas::mouseDoubleClickEvent(QMouseEvent *ev)
{
    ev->accept();

    DENG2_FOR_AUDIENCE2(MouseEvent, i)
    {
        i->mouseEvent(MouseEvent(translateButton(ev->button()), MouseEvent::DoubleClick,
                                 d->translatePosition(ev)));
    }
}

void Canvas::mouseMoveEvent(QMouseEvent *ev)
{
    ev->accept();

    // Absolute events are only emitted when the mouse is untrapped.
    if (!d->mouseGrabbed)
    {
        DENG2_FOR_AUDIENCE2(MouseEvent, i)
        {
            i->mouseEvent(MouseEvent(MouseEvent::Absolute,
                                     d->translatePosition(ev)));
        }
    }
}

void Canvas::wheelEvent(QWheelEvent *ev)
{
    ev->accept();

    float const devicePixels = d->parent->devicePixelRatio();

    QPoint numPixels = ev->pixelDelta();
    QPoint numDegrees = ev->angleDelta() / 8;
    d->wheelAngleAccum += numDegrees;

    if (!numPixels.isNull())
    {
        DENG2_FOR_AUDIENCE2(MouseEvent, i)
        {
            if (numPixels.x())
            {
                i->mouseEvent(MouseEvent(MouseEvent::FineAngle, Vector2i(devicePixels * numPixels.x(), 0),
                                         d->translatePosition(ev)));
            }
            if (numPixels.y())
            {
                i->mouseEvent(MouseEvent(MouseEvent::FineAngle, Vector2i(0, devicePixels * numPixels.y()),
                                         d->translatePosition(ev)));
            }
        }
    }

    QPoint const steps = d->wheelAngleAccum / 15;
    if (!steps.isNull())
    {
        DENG2_FOR_AUDIENCE2(MouseEvent, i)
        {
            if (steps.x())
            {
                i->mouseEvent(MouseEvent(MouseEvent::Step, Vector2i(steps.x(), 0),
                                         !d->mouseGrabbed? d->translatePosition(ev) : Vector2i()));
            }
            if (steps.y())
            {
                i->mouseEvent(MouseEvent(MouseEvent::Step, Vector2i(0, steps.y()),
                                         !d->mouseGrabbed? d->translatePosition(ev) : Vector2i()));
            }
        }
        d->wheelAngleAccum -= steps * 15;
    }

    d->prevWheelAt.start();
}

} // namespace de
