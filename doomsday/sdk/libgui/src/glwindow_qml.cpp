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
#include <QThread>

#if !defined (DENG_MOBILE)
#  error "glwindow_qml.cpp is only for mobile platforms"
#endif

namespace de {

static GLWindow *mainWindow = nullptr;

DENG2_PIMPL(GLWindow)
{
    QQuickWindow *qtWindow = nullptr;
    QOpenGLContext *mainContext = nullptr;
    GLFramebuffer backing; // Represents QOpenGLWindow's framebuffer.
    WindowEventHandler *handler = nullptr; ///< Event handler.
    bool readyNotified = false;
    Size currentSize;
    Size pendingSize;

    unsigned int frameCount = 0;
    float fps = 0;

    Impl(Public *i) : Base(i) {}

    ~Impl()
    {
        glDeinit();
    }

    void glInit()
    {
        GLInfo::glInit();
        self().Asset::setState(Ready);
    }

    void glDeinit()
    {
        self().Asset::setState(NotReady);
        readyNotified = false;
        GLInfo::glDeinit();
    }

    void notifyReady()
    {
        if (readyNotified) return;
        readyNotified = true;

        DENG2_ASSERT_IN_MAIN_THREAD();
        DENG2_ASSERT(QOpenGLContext::currentContext() == mainContext);

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
    //DENG2_ASSERT(QOpenGLContext::currentContext() != nullptr);
    if (App::inMainThread())
    {
        DENG2_ASSERT(!QOpenGLContext::currentContext() || QOpenGLContext::currentContext() == d->mainContext);
        d->mainContext->makeCurrent(d->qtWindow);
    }
    else
    {
        //d->qtWindow->openglContext()->makeCurrent(d->qtWindow);
    }
}

void GLWindow::doneCurrent()
{
}

void GLWindow::setWindow(QQuickWindow *window)
{
    d->qtWindow = window;
}

void GLWindow::setOpenGLContext(QOpenGLContext *context)
{
    d->mainContext = context;
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

bool GLWindow::isVisible() const
{
    return true;
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

void GLWindow::initializeGL()
{
    LOG_AS("GLWindow");
    LOGDEV_GL_NOTE("Initializing OpenGL window");

    DENG2_ASSERT_IN_MAIN_THREAD();
    makeCurrent();

    d->glInit();
    d->notifyReady();
}

void GLWindow::paintGL()
{
    DENG2_ASSERT_IN_RENDER_THREAD();
    DENG2_ASSERT(d->qtWindow != nullptr);
    DENG2_ASSERT(QOpenGLContext::currentContext() == d->qtWindow->openglContext());

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

    GLState::considerNativeStateUndefined();
    GLState::current().apply();

    // Make sure any changes to the state stack are in effect.
    GLState::current().target().glBind();

    draw();

    LIBGUI_ASSERT_GL_OK();

    d->qtWindow->resetOpenGLState();
}

void GLWindow::windowAboutToClose()
{}

void GLWindow::resizeGL(int w, int h)
{
    DENG2_ASSERT_IN_MAIN_THREAD();

    d->pendingSize = Size(w, h);

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

void GLWindow::frameWasSwapped()
{
    Loop::mainCall([this] ()
    {
        makeCurrent();
        DENG2_FOR_AUDIENCE2(Swap, i)
        {
            i->windowSwapped(*this);
        }
        d->updateFrameRateStatistics();
    });
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

//----------------------------------------------------------------------------------------

DENG2_PIMPL_NOREF(GLQuickItem)
{
    QQuickWindow *qtWindow = nullptr;
    GLWindow *renderer = nullptr;
    bool initPending = false;
    int touchId = 0;
};

GLQuickItem::GLQuickItem()
    : d(new Impl)
{
    connect(this, &QQuickItem::windowChanged, this, &GLQuickItem::handleWindowChanged);
}

void GLQuickItem::handleWindowChanged(QQuickWindow *win)
{
    d->qtWindow = win;

    if (win)
    {
        connect(win, &QQuickWindow::beforeSynchronizing,   this, &GLQuickItem::sync,    Qt::DirectConnection);
        connect(win, &QQuickWindow::sceneGraphInvalidated, this, &GLQuickItem::cleanup, Qt::DirectConnection);

        win->setClearBeforeRendering(false);

        if (d->renderer)
        {
            d->renderer->setWindow(win);
        }
    }
}

void GLQuickItem::sync()
{
    DENG2_ASSERT(d->qtWindow);

    if (!d->renderer && !d->initPending)
    {
        d->initPending = true;
        GuiApp::setRenderThread(QThread::currentThread());

        QOpenGLContext *renderContext = QOpenGLContext::currentContext();

        Loop::mainCall([this, renderContext] ()
        {
            LOG_AS("GLWindow");
            LOGDEV_GL_NOTE("Initializing OpenGL window");

            DENG2_ASSERT_IN_MAIN_THREAD();

            // Create a shared OpenGL context for the main thread.
            QOpenGLContext *mainContext = new QOpenGLContext;
            mainContext->setShareContext(renderContext);
            mainContext->create();

            auto *renderer = makeWindowRenderer();
            renderer->setOpenGLContext(mainContext);
            renderer->setWindow(d->qtWindow);
            renderer->initializeGL();

            connect(renderer, &GLWindow::textEntryRequest, this, &GLQuickItem::textEntryRequest);
            connect(renderer, &GLWindow::textEntryDismiss, this, &GLQuickItem::textEntryDismiss);

            d->renderer = renderer;
        });
    }

    if (d->renderer)
    {
        if (d->initPending)
        {
            d->initPending = false;

            // Painting may commence.
            connect(d->qtWindow, &QQuickWindow::beforeRendering, d->renderer, &GLWindow::paintGL,         Qt::DirectConnection);
            connect(d->qtWindow, &QQuickWindow::frameSwapped,    d->renderer, &GLWindow::frameWasSwapped, Qt::DirectConnection);
        }

        QSize const winSize = d->qtWindow->size() * d->qtWindow->devicePixelRatio();
        Loop::mainCall([this, winSize] ()
        {
            d->renderer->resizeGL(winSize.width(), winSize.height());
        });
    }
}

void GLQuickItem::cleanup()
{
    delete d->renderer;
    d->renderer = nullptr;
    d->initPending = false;
}

void GLQuickItem::dimensionsChanged()
{
    if (d->renderer && d->qtWindow)
    {
        auto const ratio = d->qtWindow->devicePixelRatio();

        QRect newRect = QRect(0, 0, width() * ratio, height() * ratio);
        qDebug() << "dimensions" << newRect;

        // Just resize the root widget, the window hasn't changed.
        emit d->renderer->rootDimensionsChanged(newRect);
    }
}

void GLQuickItem::userEnteredText(QString text)
{
    if (d->renderer)
    {
        qDebug() << "user entered:" << text;
        emit d->renderer->userEnteredText(text);
    }
}

void GLQuickItem::userFinishedTextEntry()
{
    if (d->renderer)
    {
        qDebug() << "user Done";

        auto &handler = d->renderer->eventHandler();
//        QKeyEvent(Type type, int key, Qt::KeyboardModifiers modifiers, const QString& text = QString(),
//                  bool autorep = false, ushort count = 1);

        // Simulate the press of the Enter key.

        QKeyEvent pressed (QEvent::KeyPress,   Qt::Key_Enter, Qt::NoModifier, "\n");
        QKeyEvent released(QEvent::KeyRelease, Qt::Key_Enter, Qt::NoModifier);

        handler.keyPressEvent(&pressed);
        handler.keyReleaseEvent(&released);

        emit d->renderer->userFinishedTextEntry();
    }
}

void GLQuickItem::onTouchPressed(QVariantList touchPoints)
{
    if (!d->renderer) return;

    //qDebug() << "GLQuickItem: onTouchPressed" << touchPoints;
    foreach (QVariant item, touchPoints)
    {
        QObject *obj = item.value<QObject *>();
        int id = obj->property("pointId").toInt();
        if (id == 0)
        {
            QMouseEvent event(QEvent::MouseButtonPress,
                              QPointF(obj->property("x").toDouble(),
                                      obj->property("y").toDouble()),
                              Qt::LeftButton,
                              Qt::LeftButton,
                              Qt::NoModifier);
            d->renderer->eventHandler().mousePressEvent(&event);
        }
    }
}

void GLQuickItem::onTouchUpdated(QVariantList touchPoints)
{
    if (!d->renderer) return;

    foreach (QVariant item, touchPoints)
    {
        QObject *obj = item.value<QObject *>();
        if (obj->property("pointId").toInt() == 0)
        {
            QMouseEvent event(QEvent::MouseMove,
                              QPointF(obj->property("x").toDouble(),
                                      obj->property("y").toDouble()),
                              Qt::LeftButton,
                              Qt::LeftButton,
                              Qt::NoModifier);
            d->renderer->eventHandler().mouseMoveEvent(&event);
        }
    }
}

void GLQuickItem::onTouchReleased(QVariantList touchPoints)
{
    if (!d->renderer) return;

    foreach (QVariant item, touchPoints)
    {
        QObject *obj = item.value<QObject *>();
        if (obj->property("pointId").toInt() == 0)
        {
            QMouseEvent event(QEvent::MouseButtonRelease,
                              QPointF(obj->property("x").toDouble(),
                                      obj->property("y").toDouble()),
                              Qt::LeftButton,
                              Qt::NoButton,
                              Qt::NoModifier);
            d->renderer->eventHandler().mouseReleaseEvent(&event);
        }
    }
}

} // namespace de
