/** @file canvas.cpp  OpenGL drawing surface (QWidget).
 *
 * @authors Copyright (c) 2012-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "de/Canvas"
#include "de/CanvasWindow"

#include <de/App>
#include <de/Log>

#include <QApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QShowEvent>
#include <QResizeEvent>
#include <QPaintEvent>
#include <QtOpenGL>
#include <QImage>
#include <QCursor>
#include <QTimer>
#include <QTime>
#include <QDebug>

namespace de {

static const int MOUSE_WHEEL_CONTINUOUS_THRESHOLD_MS = 100;

DENG2_PIMPL(Canvas)
{
    CanvasWindow *parent;
    bool readyNotified;
    Vector2i currentSize;
    bool mouseDisabled;
    bool mouseGrabbed;
#ifdef WIN32
    bool altIsDown;
#endif
    QPoint prevMousePos;
    QTime prevWheelAt;
    int wheelDir[2];

    Instance(Public *i, CanvasWindow *parentWindow)
        : Base(i),
          parent(parentWindow),
          readyNotified(false),
          mouseDisabled(false),
          mouseGrabbed(false)
    {
        wheelDir[0] = wheelDir[1] = 0;
#ifdef WIN32
        altIsDown = false;
#endif

        mouseDisabled = App::commandLine().has("-nomouse");
    }

    void grabMouse()
    {
        if(!self.isVisible() || mouseDisabled) return;

        LOG_DEBUG("grabbing mouse (already grabbed? %b)") << mouseGrabbed;

        if(!mouseGrabbed)
        {
            mouseGrabbed = true;

            DENG2_FOR_PUBLIC_AUDIENCE(MouseStateChange, i)
            {
                i->mouseStateChanged(Trapped);
            }
        }
    }

    void ungrabMouse()
    {
        if(!self.isVisible() || mouseDisabled) return;

        LOG_DEBUG("ungrabbing mouse (presently grabbed? %b)") << mouseGrabbed;

        if(mouseGrabbed)
        {
            // Tell the mouse driver that the mouse is untrapped.
            mouseGrabbed = false;

            DENG2_FOR_PUBLIC_AUDIENCE(MouseStateChange, i)
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
        if(ev->isAutoRepeat()) return; // Ignore repeats, we do our own.

        /*
        qDebug() << "Canvas: key press" << ev->key() << QString("0x%1").arg(ev->key(), 0, 16)
                 << "text:" << ev->text()
                 << "native:" << ev->nativeVirtualKey()
                 << "scancode:" << ev->nativeScanCode();
        */

#ifdef WIN32
        // We must track the state of the alt key ourselves as the OS grabs the up event...
        if(ev->key() == Qt::Key_Alt)
        {
            if(ev->type() == QEvent::KeyPress)
            {
                if(altIsDown) return; // Ignore repeat down events(!)?
                altIsDown = true;
            }
            else if(ev->type() == QEvent::KeyRelease)
            {
                if(!altIsDown)
                {
                    LOG_DEBUG("Ignoring repeat alt up.");
                    return; // Ignore repeat up events.
                }
                altIsDown = false;
                //LOG_DEBUG("Alt is up.");
            }
        }
#endif

        DENG2_FOR_PUBLIC_AUDIENCE(KeyEvent, i)
        {
            i->keyEvent(KeyEvent(ev->type() == QEvent::KeyPress? KeyEvent::Pressed :
                                                                 KeyEvent::Released,
                                 ev->key(),
                                 KeyEvent::ddKeyFromQt(ev->key(), ev->nativeVirtualKey(), ev->nativeScanCode()),
                                 nativeCode(ev),
                                 ev->text()));
        }
    }
};

Canvas::Canvas(CanvasWindow* parent, QGLWidget* shared)
    : QGLWidget(parent, shared), d(new Instance(this, parent))
{
    LOG_AS("Canvas");
    LOG_DEBUG("swap interval: ") << format().swapInterval();
    LOG_DEBUG("multisample: %b") << format().sampleBuffers();

    // We will be doing buffer swaps manually (for timing purposes).
    setAutoBufferSwap(false);

    setFocusPolicy(Qt::StrongFocus);
}

void Canvas::setParent(CanvasWindow *parent)
{
    d->parent = parent;
}

QImage Canvas::grabImage(QSize const &outputSize)
{
    // We will be grabbing the visible, latest complete frame.
    glReadBuffer(GL_FRONT);
    QImage grabbed = grabFrameBuffer(); // no alpha
    glReadBuffer(GL_BACK);
    if(outputSize.isValid())
    {
        grabbed = grabbed.scaled(outputSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    return grabbed;
}

GLuint Canvas::grabAsTexture(QSize const &outputSize)
{
    return bindTexture(grabImage(outputSize), GL_TEXTURE_2D, GL_RGB,
                       QGLContext::LinearFilteringBindOption);
}

Vector2i Canvas::size() const
{
    return d->currentSize;
}

void Canvas::trapMouse(bool trap)
{
    if(d->mouseDisabled) return;

    if(trap)
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

void Canvas::copyAudiencesFrom(Canvas const &other)
{
    audienceForGLReady          = other.audienceForGLReady;
    audienceForGLInit           = other.audienceForGLInit;
    audienceForGLResize         = other.audienceForGLResize;
    audienceForGLDraw           = other.audienceForGLDraw;
    audienceForFocusChange      = other.audienceForFocusChange;

    audienceForKeyEvent         = other.audienceForKeyEvent;

    audienceForMouseStateChange = other.audienceForMouseStateChange;
    audienceForMouseAxisEvent   = other.audienceForMouseAxisEvent;
    audienceForMouseButtonEvent = other.audienceForMouseButtonEvent;
}

void Canvas::initializeGL()
{
    LOG_AS("Canvas");
    LOG_DEBUG("Notifying GL init (during paint)");

    DENG2_FOR_AUDIENCE(GLInit, i) i->canvasGLInit(*this);
}

void Canvas::resizeGL(int w, int h)
{
    Vector2i newSize(w, h);

    // Only react if this is actually a resize.
    if(d->currentSize != newSize)
    {
        d->currentSize = newSize;

        DENG2_FOR_AUDIENCE(GLResize, i) i->canvasGLResized(*this);
    }
}

void Canvas::showEvent(QShowEvent* ev)
{
    LOG_AS("Canvas");

    QGLWidget::showEvent(ev);

    // The first time the window is shown, run the initialization callback. On
    // some platforms, OpenGL is not fully ready to be used before the window
    // actually appears on screen.
    if(isVisible() && !d->readyNotified)
    {
        LOG_DEBUG("Received first show event, scheduling GL ready notification");

        QTimer::singleShot(1, this, SLOT(notifyReady()));
    }
}

void Canvas::notifyReady()
{
    if(d->readyNotified) return;

    d->readyNotified = true;

    LOG_AS("Canvas");
    LOG_DEBUG("Notifying GL ready");

    DENG2_FOR_AUDIENCE(GLReady, i) i->canvasGLReady(*this);

    // This Canvas instance might have been destroyed now.
}

void Canvas::paintGL()
{
    if(!audienceForGLDraw.isEmpty())
    {
        DENG2_FOR_AUDIENCE(GLDraw, i) i->canvasGLDraw(*this);
    }
    else
    {
        LOG_AS("Canvas");
        LOG_TRACE("Drawing with default paint func.");

        // Since we don't know what else to draw, just draw blackness.
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);

        swapBuffers();
    }
}

void Canvas::focusInEvent(QFocusEvent*)
{
    LOG_AS("Canvas");
    LOG_INFO("Gained focus.");

    DENG2_FOR_AUDIENCE(FocusChange, i) i->canvasFocusChanged(*this, true);
}

void Canvas::focusOutEvent(QFocusEvent*)
{
    LOG_AS("Canvas");
    LOG_INFO("Lost focus.");

    // Automatically ungrab the mouse if focus is lost.
    d->ungrabMouse();

    DENG2_FOR_AUDIENCE(FocusChange, i) i->canvasFocusChanged(*this, false);
}

void Canvas::keyPressEvent(QKeyEvent *ev)
{
    d->handleKeyEvent(ev);
}

void Canvas::keyReleaseEvent(QKeyEvent *ev)
{
    d->handleKeyEvent(ev);
}

static MouseEventSource::Button translateButton(Qt::MouseButton btn)
{
    if(btn == Qt::LeftButton)   return MouseEventSource::Left;
#ifdef DENG2_QT_4_7_OR_NEWER
    if(btn == Qt::MiddleButton) return MouseEventSource::Middle;
#else
    if(btn == Qt::MidButton)    return MouseEventSource::Middle;
#endif
    if(btn == Qt::RightButton)  return MouseEventSource::Right;
    if(btn == Qt::XButton1)     return MouseEventSource::XButton1;
    if(btn == Qt::XButton2)     return MouseEventSource::XButton2;

    return MouseEventSource::Unknown;
}

void Canvas::mousePressEvent(QMouseEvent *ev)
{
    if(!d->mouseGrabbed)
    {
        // The mouse will be grabbed when the button is released.
        ev->ignore();
        return;
    }

    ev->accept();

    DENG2_FOR_AUDIENCE(MouseButtonEvent, i)
    {
        i->mouseButtonEvent(translateButton(ev->button()), MouseEventSource::Pressed);
    }
}

void Canvas::mouseReleaseEvent(QMouseEvent* ev)
{
    if(d->mouseDisabled)
    {
        ev->ignore();
        return;
    }

    ev->accept();

    if(!d->mouseGrabbed)
    {
        // Start grabbing after a click.
        trapMouse();
        return;
    }

    DENG2_FOR_AUDIENCE(MouseButtonEvent, i)
    {
        i->mouseButtonEvent(translateButton(ev->button()), MouseEventSource::Released);
    }
}

void Canvas::wheelEvent(QWheelEvent *ev)
{
    if(d->mouseDisabled)
    {
        ev->ignore();
        return;
    }

    ev->accept();

    bool continuousMovement = (d->prevWheelAt.elapsed() < MOUSE_WHEEL_CONTINUOUS_THRESHOLD_MS);
    int axis = (ev->orientation() == Qt::Horizontal? 0 : 1);
    int dir = (ev->delta() < 0? -1 : 1);

    if(!continuousMovement || d->wheelDir[axis] != dir)
    {
        d->wheelDir[axis] = dir;
        //qDebug() << "Canvas: signal wheel axis" << axis << "dir" << dir;

        DENG2_FOR_AUDIENCE(MouseAxisEvent, i)
        {
            i->mouseAxisEvent(MouseEventSource::Wheel,
                              axis == 0? Vector2i(dir, 0) :
                              axis == 1? Vector2i(0, dir) :
                                         Vector2i());
        }
    }

    d->prevWheelAt.start();
}

} // namespace de
