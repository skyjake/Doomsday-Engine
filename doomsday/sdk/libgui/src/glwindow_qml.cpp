/** @file glwindow.cpp  Top-level OpenGL window (QML item).
 * @ingroup base
 *
 * @authors Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/GLWindow"
#include "de/GuiApp"

#include <de/GLBuffer>
#include <de/GLState>
#include <de/GLFramebuffer>
#include <de/Log>
#include <de/c_wrapper.h>

#include <QElapsedTimer>
#include <QImage>
#include <QQuickWindow>
#include <QSurfaceFormat>
#include <QTimer>

namespace de {

static GLWindow *mainWindow = nullptr;

DENG2_PIMPL(GLWindow)
{
    QQuickWindow *qtWindow = nullptr;
    LoopCallback mainCall;
    GLFramebuffer backing; // Represents QOpenGLWindow's framebuffer.
    WindowEventHandler *handler = nullptr; ///< Event handler.
    bool readyPending = false;
    bool readyNotified = false;
    Size currentSize;
    Size pendingSize;

    unsigned int frameCount = 0;
    float fps = 0;

    Impl(Public *i) : Base(i) {}

    void glInit()
    {
        GLInfo::glInit();
        self().Asset::setState(Ready);
    }

    void glDeinit()
    {
        self().Asset::setState(NotReady);
        readyNotified = false;
        readyPending = false;
        GLInfo::glDeinit();
    }

    void notifyReady()
    {
        if (readyNotified) return;

        readyPending = false;

        DENG2_ASSERT(QOpenGLContext::currentContext() != nullptr);

        // Print some information.
        QSurfaceFormat const fmt = qtWindow->format();

#if defined (DENG_OPENGL)
        LOG_GL_NOTE("OpenGL %i.%i supported%s")
                << fmt.majorVersion()
                << fmt.minorVersion()
                << (fmt.majorVersion() > 2?
                        (fmt.profile() == QSurfaceFormat::CompatibilityProfile? " (Compatibility)"
                                                                              : " (Core)") : "");
#else
        LOG_GL_NOTE("OpenGL ES %i.%i supported")
                << fmt.majorVersion() << fmt.minorVersion();
#endif

        // Everybody can perform GL init now.
        DENG2_FOR_PUBLIC_AUDIENCE2(Init, i) i->windowInit(self());

        readyNotified = true;

        // Now we can paint.
        //mainCall.enqueue([this] () { self().update(); });
    }

    void updateFrameRateStatistics()
    {
        static Time lastFpsTime;

        Time const nowTime = Clock::appTime();

        // Increment the (local) frame counter.
        frameCount++;

        // Count the frames every other second.
        TimeDelta elapsed = nowTime - lastFpsTime;
        if (elapsed > 2.5)
        {
            fps = frameCount / elapsed;
            lastFpsTime = nowTime;
            frameCount = 0;
        }
    }

    DENG2_PIMPL_AUDIENCE(Init)
    DENG2_PIMPL_AUDIENCE(Resize)
    DENG2_PIMPL_AUDIENCE(Swap)
};

DENG2_AUDIENCE_METHOD(GLWindow, Init)
DENG2_AUDIENCE_METHOD(GLWindow, Resize)
DENG2_AUDIENCE_METHOD(GLWindow, Swap)

GLWindow::GLWindow()
    : d(new Impl(this))
{
    // Create the drawing canvas for this window.
    d->handler = new WindowEventHandler(this);
    
    connect(this, &QQuickItem::windowChanged, this, &GLWindow::handleWindowChanged);
}

void GLWindow::setTitle(QString const &title)
{
    //setWindowTitle(title);
}
    
QSurfaceFormat GLWindow::format() const
{
    DENG2_ASSERT(d->qtWindow);
    return d->qtWindow->format();
}
    
double GLWindow::devicePixelRatio() const
{
    DENG2_ASSERT(d->qtWindow);
    return d->qtWindow->devicePixelRatio();
}
    
void GLWindow::makeCurrent()
{
    DENG2_ASSERT(QOpenGLContext::currentContext() != nullptr);
}
    
void GLWindow::doneCurrent()
{
        
}
    
void GLWindow::update()
{
    if (d->qtWindow) d->qtWindow->update();
}
    
Rectanglei GLWindow::windowRect() const
{
    Size const size = pointSize();
    return Rectanglei(0, 0, size.x, size.y);
}
    
GLWindow::Size GLWindow::fullscreenSize() const
{
    return pointSize();
}
    
void GLWindow::hide()
{
    
}
    
bool GLWindow::isGLReady() const
{
    return d->readyNotified;
}

bool GLWindow::isMaximized() const
{
    return false;
}

bool GLWindow::isMinimized() const
{
    return false;
}

bool GLWindow::isFullScreen() const
{
    return true;
}

bool GLWindow::isHidden() const
{
    return false;
}

GLFramebuffer &GLWindow::framebuffer() const
{
    return d->backing;
}

float GLWindow::frameRate() const
{
    return d->fps;
}

uint GLWindow::frameCount() const
{
    return d->frameCount;
}

GLWindow::Size GLWindow::pointSize() const
{
    if (!d->qtWindow) return Size();
    return Size(duint(de::max(0, d->qtWindow->width())),
                duint(de::max(0, d->qtWindow->height())));
}

GLWindow::Size GLWindow::pixelSize() const
{
    return d->currentSize;
}

int GLWindow::pointWidth() const
{
    return pointSize().x;
}

int GLWindow::pointHeight() const
{
    return pointSize().y;
}

int GLWindow::pixelWidth() const
{
    return pixelSize().x;
}

int GLWindow::pixelHeight() const
{
    return pixelSize().y;
}

WindowEventHandler &GLWindow::eventHandler() const
{
    DENG2_ASSERT(d->handler != 0);
    return *d->handler;
}

bool GLWindow::ownsEventHandler(WindowEventHandler *handler) const
{
    if (!handler) return false;
    return d->handler == handler;
}

    /*
void GLWindow::focusInEvent(QFocusEvent *ev)
{
    d->handler->focusInEvent(ev);
}

void GLWindow::focusOutEvent(QFocusEvent *ev)
{
    d->handler->focusOutEvent(ev);
}

void GLWindow::keyPressEvent(QKeyEvent *ev)
{
    d->handler->keyPressEvent(ev);
}

void GLWindow::keyReleaseEvent(QKeyEvent *ev)
{
    d->handler->keyReleaseEvent(ev);
}

void GLWindow::mousePressEvent(QMouseEvent *ev)
{
    d->handler->mousePressEvent(ev);
}

void GLWindow::mouseReleaseEvent(QMouseEvent *ev)
{
    d->handler->mouseReleaseEvent(ev);
}

void GLWindow::mouseDoubleClickEvent(QMouseEvent *ev)
{
    d->handler->mouseDoubleClickEvent(ev);
}

void GLWindow::mouseMoveEvent(QMouseEvent *ev)
{
    d->handler->mouseMoveEvent(ev);
}

void GLWindow::wheelEvent(QWheelEvent *ev)
{
    d->handler->wheelEvent(ev);
}

bool GLWindow::event(QEvent *ev)
{
    if (ev->type() == QEvent::Close)
    {
        windowAboutToClose();
    }
    return LIBGUI_GLWINDOW_BASECLASS::event(ev);
}
     */

bool GLWindow::grabToFile(NativePath const &path) const
{
    return grabImage().save(path.toString());
}

QImage GLWindow::grabImage(QSize const &outputSize) const
{
    Size const size = pixelSize();
    return grabImage(QRect(QPoint(0, 0), QSize(size.x, size.y)), outputSize);
}

QImage GLWindow::grabImage(QRect const &area, QSize const &outputSize) const
{
#if 0
    // We will be grabbing the visible, latest complete frame.
    //LIBGUI_GL.glReadBuffer(GL_FRONT);
    QImage grabbed = const_cast<GLWindow *>(this)->grabFramebuffer(); // no alpha
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
#endif
    return QImage();
}

void GLWindow::glActivate()
{
    if (isGLReady())
    {
        makeCurrent();
    }
    //d->qtWindow->makeCurrent();
    //d->qtWindow->openglContext()
}

void GLWindow::glDone()
{
    //doneCurrent();
}

void *GLWindow::nativeHandle() const
{
#if 0
    return reinterpret_cast<void *>(winId());
#endif
    return nullptr;
}

/*void GLWindow::initializeGL()
{
    LOG_AS("GLWindow");
    LOGDEV_GL_NOTE("Initializing OpenGL window");

    d->glInit();
}*/

void GLWindow::paintGL()
{
    DENG2_ASSERT(d->qtWindow != nullptr);
    DENG2_ASSERT(QOpenGLContext::currentContext() != nullptr);
    
    GLFramebuffer::setDefaultFramebuffer(QOpenGLContext::currentContext()->defaultFramebufferObject());

    // Do not proceed with painting until after the application has completed
    // GL initialization. This is done via timer callback because we don't
    // want to perform a long-running operation during a paint event.

    /*if (!d->readyNotified)
    {
        if (!d->readyPending)
        {
            d->readyPending = true;
            d->mainCall.enqueue([this] () { d->notifyReady(); });
        }
        LIBGUI_GL.glClear(GL_COLOR_BUFFER_BIT);
        return;
    }*/

    GLBuffer::resetDrawCount();

    LIBGUI_ASSERT_GL_OK();

    // Make sure any changes to the state stack are in effect.
    GLState::current().target().glBind();

    draw();

    LIBGUI_ASSERT_GL_OK();
    
    d->qtWindow->resetOpenGLState();
}

void GLWindow::windowAboutToClose()
{}

/*
void GLWindow::resizeEvent(QResizeEvent *ev)
{
    d->pendingSize = Size(ev->size().width(), ev->size().height()) * qApp->devicePixelRatio();

    // Only react if this is actually a resize.
    if (d->currentSize != d->pendingSize)
    {
        d->currentSize = d->pendingSize;

        if (d->readyNotified)
        {
            makeCurrent();
        }

        DENG2_FOR_AUDIENCE2(Resize, i) i->windowResized(*this);

        if (d->readyNotified)
        {
            doneCurrent();
        }
    }
}
*/
    
void GLWindow::frameWasSwapped()
{
    makeCurrent();
    DENG2_FOR_AUDIENCE2(Swap, i)
    {
        i->windowSwapped(*this);
    }
    d->updateFrameRateStatistics();
    //doneCurrent();
}

void GLWindow::handleWindowChanged(QQuickWindow *win)
{
    d->qtWindow = win;
    
    if (win)
    {
        connect(win, &QQuickWindow::frameSwapped,          this, &GLWindow::frameWasSwapped, Qt::DirectConnection);
        connect(win, &QQuickWindow::beforeSynchronizing,   this, &GLWindow::sync,            Qt::DirectConnection);
        connect(win, &QQuickWindow::sceneGraphInvalidated, this, &GLWindow::cleanup,         Qt::DirectConnection);
        
        win->setClearBeforeRendering(false);
    }
}
    
void GLWindow::sync()
{
    DENG2_ASSERT(d->qtWindow);
    
    if (!d->readyNotified)
    {
        LOG_AS("GLWindow");
        LOGDEV_GL_NOTE("Initializing OpenGL window");
        
        d->glInit();
        d->notifyReady();
        
        connect(d->qtWindow, &QQuickWindow::beforeRendering, this, &GLWindow::paintGL, Qt::DirectConnection);
    }
    
    QSize const winSize = d->qtWindow->size() * d->qtWindow->devicePixelRatio();
    d->pendingSize = Size(winSize.width(), winSize.height());
    
    // Only react if this is actually a resize.
    if (d->currentSize != d->pendingSize)
    {
        makeCurrent();
        d->currentSize = d->pendingSize;
        DENG2_FOR_AUDIENCE2(Resize, i) i->windowResized(*this);
    }
}

void GLWindow::cleanup()
{
    if (isReady())
    {
        d->glDeinit();
    }
}
    
bool GLWindow::mainExists() // static
{
    return mainWindow != 0;
}

GLWindow &GLWindow::main() // static
{
    DENG2_ASSERT(mainWindow != 0);
    return *mainWindow;
}

void GLWindow::glActiveMain()
{
    if (mainExists()) main().glActivate();
}

void GLWindow::setMain(GLWindow *window) // static
{
    mainWindow = window;
    GuiLoop::get().setWindow(window);
}

} // namespace de
