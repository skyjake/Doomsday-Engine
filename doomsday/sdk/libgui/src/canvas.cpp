/** @file canvas.cpp  OpenGL drawing surface (QWidget).
 *
 * @authors Copyright (c) 2012-2015 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "de/GLState"
#include "de/GLTexture"
#include "de/QtInputSource"
#include "de/graphics/opengl.h"
#include "de/gui/canvas_macx.h"

#include <de/App>
#include <de/Log>
#include <de/Drawable>
#include <de/GLInfo>
#include <de/Loop>
#include <de/GLFramebuffer>

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

namespace de {

/*
#ifdef DENG_X11
#  define LIBGUI_CANVAS_USE_DEFERRED_RESIZE
#endif
*/

static Canvas *mainCanvas = 0;

DENG2_PIMPL(Canvas)
{
    QtInputSource input;
    GLFramebuffer framebuf; ///< Represents the default FBO (the window surface).
    unsigned int frameCount = 0;
    float fps = 0;

    Size currentSize;
    /*
    Size pendingSize;
#ifdef LIBGUI_CANVAS_USE_DEFERRED_RESIZE
    QTimer resizeTimer;
#endif*/

    Instance(Public *i) : Base(i), input(*i)
    {        
        self.installEventFilter(&input);

        /*
#ifdef LIBGUI_CANVAS_USE_DEFERRED_RESIZE
        resizeTimer.setSingleShot(true);
        QObject::connect(&resizeTimer, SIGNAL(timeout()), thisPublic, SLOT(updateSize()));
#endif
*/
    }

    ~Instance()
    {
        self.removeEventFilter(&input);

        glDeinit();
        if(thisPublic == mainCanvas)
        {
            mainCanvas = nullptr;
        }
    }

    void makeReady()
    {
        if(self.isReady()) return;
        
        qDebug() << "Canvas: making ready";

        glInit();
        //d->reconfigureFramebuffer();

        // Print some information.
        QSurfaceFormat const fmt = self.format();

        if(fmt.version() < qMakePair(2, 0))
        {
            LOG_GL_WARNING("OpenGL 2.0 is not supported!");
        }
        else
        {
            LOG_GL_NOTE("OpenGL %i.%i supported")
                    << fmt.majorVersion() << fmt.minorVersion();
        }

        //DENG2_FOR_AUDIENCE2(GLReady, i) i->canvasGLReady(*this);

        LOGDEV_GL_XVERBOSE("Notifying GL ready");
        self.setState(true);
    }

    /*
    void reconfigureFramebuffer()
    {
        //framebuf.setColorFormat(Image::RGB_888);
        //framebuf.resize(currentSize);
    }*/

    void glInit()
    {
        //DENG2_ASSERT(parent != 0);
        framebuf.glInit();
    }

    void glDeinit()
    {
        framebuf.glDeinit();
    }

#if 0
    void swapBuffers(gl::SwapBufferMode mode)
    {
        if(mode == gl::SwapStereoBuffers && !self.format().stereo())
        {
            // The canvas is not using a stereo format, must do a normal swap.
            mode = gl::SwapMonoBuffer;
        }

        /// @todo Double buffering is not really needed in manual FB mode.
        framebuf.swapBuffers(self, mode);
    }
#endif

    void updateFrameRateStatistics(void)
    {
        static Time lastFpsTime;

        Time const nowTime = Clock::appTime();

        // Increment the (local) frame counter.
        frameCount++;

        // Count the frames every other second.
        TimeDelta elapsed = nowTime - lastFpsTime;
        if(elapsed > 2.5)
        {
            fps = frameCount / elapsed;
            lastFpsTime = nowTime;
            frameCount = 0;
        }
    }

    void finishCanvasRecreation()
    {
        DENG2_ASSERT_IN_MAIN_THREAD();


#if 0

        LOGDEV_GL_MSG("About to replace Canvas %p with %p")
                << de::dintptr(canvas) << de::dintptr(recreated);

        // Copy the audiences of the old canvas.
        recreated->copyAudiencesFrom(*canvas);

        // Switch the central widget. This will delete the old canvas automatically.
        self.setCentralWidget(recreated);
        canvas = recreated;
        recreated = 0;

        // Set up the basic GL state for the new canvas.
        canvas->makeCurrent();
        LIBGUI_ASSERT_GL_OK();

        DENG2_FOR_EACH_OBSERVER(Canvas::GLInitAudience, i, canvas->audienceForGLInit())
        {
            i->canvasGLInit(*canvas);
        }

        DENG2_GUI_APP->notifyGLContextChanged();

#ifdef DENG_X11
        canvas->update();
#else
        canvas->updateGL();
#endif
        LIBGUI_ASSERT_GL_OK();

        // Reacquire the focus.
        canvas->setFocus();
        if(mouseWasTrapped)
        {
            canvas->trapMouse();
        }

        // Restore the old focus change audience.
        canvas->audienceForFocusChange() = canvasFocusAudience;

        LOGDEV_GL_MSG("Canvas replaced with %p") << de::dintptr(canvas);
#endif
    }

    DENG2_PIMPL_AUDIENCE(GLInit)
    DENG2_PIMPL_AUDIENCE(GLResize)
};

DENG2_AUDIENCE_METHOD(Canvas, GLInit)
DENG2_AUDIENCE_METHOD(Canvas, GLResize)

Canvas::Canvas() : QOpenGLWindow(), d(new Instance(this))
{
    d->framebuf.setCanvas(*this);
    
    //LOG_AS("Canvas");
    //LOGDEV_GL_VERBOSE("Swap interval: ") << format().swapInterval();
    //LOGDEV_GL_VERBOSE("Multisampling: %b") << (GLFramebuffer::defaultMultisampling() > 1);

    // We will be doing buffer swaps manually (for timing purposes).
    //setAutoBufferSwap(false);

    //setMouseTracking(true);
    //setFocusPolicy(Qt::StrongFocus);

    // TODO: Use this for loop iterations instead of a timer.
    //connect(this, SIGNAL(frameSwapped()), &Loop::get(), SLOT(nextLoopIteration()));
}

QImage Canvas::grabImage(QSize const &outputSize) const
{
    return grabImage(QRect(0, 0, width(), height()), outputSize);
}

QImage Canvas::grabImage(QRect const &area, QSize const &outputSize) const
{
    // We will be grabbing the visible, latest complete frame.
    glReadBuffer(GL_FRONT);
    QImage grabbed = const_cast<Canvas *>(this)->grabFramebuffer(); // no alpha
    if(area.size() != grabbed.size())
    {
        // Just take a portion of the full image.
        grabbed = grabbed.copy(area);
    }
    glReadBuffer(GL_BACK);
    if(outputSize.isValid())
    {
        grabbed = grabbed.scaled(outputSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    return grabbed;
}

void Canvas::grab(GLTexture &dest, QSize const &outputSize) const
{
    grab(dest, QRect(0, 0, width(), height()), outputSize);
}

void Canvas::grab(GLTexture &dest, QRect const &area, QSize const &outputSize) const
{
    dest.setImage(grabImage(area, outputSize));
}

void Canvas::grab(GLTexture &dest, GrabMode mode) const
{
    grab(dest, mode == GrabHalfSized? QSize(width()/2, height()/2) : QSize());
}

void Canvas::grab(GLTexture &dest, Rectanglei const &area, GrabMode mode) const
{
    QSize size;
    if(mode == GrabHalfSized)
    {
        size = QSize(area.width()/2, area.height()/2);
    }
    grab(dest, QRect(area.left(), area.top(), area.width(), area.height()), size);
}

bool Canvas::grabToFile(NativePath const &path) const
{
    return grabImage().save(path.toString());
}

Canvas::Size Canvas::glSize() const
{
    return d->currentSize;
}

GLFramebuffer &Canvas::framebuffer() const
{
    return d->framebuf;
}

/*void Canvas::swapBuffers(gl::SwapBufferMode swapMode)
{
    d->swapBuffers(swapMode);
}*/

void Canvas::initializeGL()
{
    LOG_AS("Canvas");
    LOGDEV_GL_NOTE("OpenGL initialization");
    
#ifdef MACOSX
    Canvas_OSX_EnableFullScreenMode(this);
#endif

#ifdef LIBGUI_USE_GLENTRYPOINTS
    getAllOpenGLEntryPoints();
#endif
    GLInfo::glInit();

    DENG2_FOR_AUDIENCE2(GLInit, i) i->canvasGLInit(*this);
}

void Canvas::resizeGL(int w, int h)
{
    w *= qApp->devicePixelRatio();
    h *= qApp->devicePixelRatio();
    
    d->currentSize = Size(max(0, w), max(0, h));

    DENG2_FOR_AUDIENCE2(GLResize, i) i->canvasGLResized(*this);

    /*
    d->pendingSize = Size(max(0, w), max(0, h));

    // Only react if this is actually a resize.
    if(d->currentSize != d->pendingSize)
    {
#ifdef LIBGUI_CANVAS_USE_DEFERRED_RESIZE
        LOGDEV_GL_MSG("Canvas %p triggered size to %ix%i from %s")
                << this << w << h << d->currentSize.asText();
        d->resizeTimer.start(100);
#else
        updateSize();
#endif
    }*/
}

#if 0
void Canvas::updateSize()
{
#ifdef LIBGUI_CANVAS_USE_DEFERRED_RESIZE
    LOGDEV_GL_MSG("Canvas %p resizing now") << this;
#endif

    makeCurrent();
    d->currentSize = d->pendingSize; 
    //d->reconfigureFramebuffer();

    DENG2_FOR_AUDIENCE2(GLResize, i) i->canvasGLResized(*this);
}
#endif

void Canvas::paintGL()
{
    DENG2_ASSERT(QGLContext::currentContext() != 0);
    LIBGUI_ASSERT_GL_OK();

    // Make sure any changes to the state stack become effective.
    GLState::current().apply();

    // Derived classes will draw contents now.
    drawCanvas();
    
    LIBGUI_ASSERT_GL_OK();
}

void Canvas::drawCanvas()
{
    d->updateFrameRateStatistics();
}

float Canvas::frameRate() const
{
    return d->fps;
}

QtInputSource &Canvas::input()
{
    return d->input;
}

QtInputSource const &Canvas::input() const
{
    return d->input;
}

void Canvas::recreateCanvas()
{
    DENG2_ASSERT_IN_MAIN_THREAD();

    GLState::considerNativeStateUndefined();

    bool const wasVisible = isVisible();

    destroy();
    create();

    if(wasVisible)
    {
        setVisible(true);
    }

    // TODO: Need to setup GL state again?

#if 0
    // Steal the focus change audience temporarily so no spurious focus
    // notifications are sent.
    d->canvasFocusAudience = canvas().audienceForFocusChange();
    canvas().audienceForFocusChange().clear();

    // We'll re-trap the mouse after the new canvas is ready.
    d->mouseWasTrapped = canvas().isMouseTrapped();
    canvas().trapMouse(false);
    canvas().setParent(0);
    canvas().hide();

    // Create the replacement Canvas. Once it's created and visible, we'll
    // finish the switch-over.
    d->recreated = new Canvas(this, d->canvas);
    d->recreated->audienceForGLReady() += this;

    //d->recreated->setGeometry(d->canvas->geometry());
    d->recreated->show();
    d->recreated->update();

    LIBGUI_ASSERT_GL_OK();

    LOGDEV_GL_MSG("Canvas recreated, old one still exists");
    qDebug() << "old Canvas" << &canvas();
    qDebug() << "new Canvas" << d->recreated;
#endif
}

/*
#ifdef WIN32
bool CanvasWindow::event(QEvent *ev)
{
    if(ev->type() == QEvent::ActivationChange)
    {
        //LOG_DEBUG("CanvasWindow: Forwarding QEvent::KeyRelease, Qt::Key_Alt");
        QKeyEvent keyEvent = QKeyEvent(QEvent::KeyRelease, Qt::Key_Alt, Qt::NoModifier);
        return QApplication::sendEvent(&canvas(), &keyEvent);
    }
    return QMainWindow::event(ev);
}
#endif
*/

/*void CanvasWindow::canvasGLReady(Canvas &canvas)
{
    d->ready = true;

    if(d->recreated == &canvas)
    {
#ifndef DENG_X11
        d->finishCanvasRecreation();
#else
        // Need to defer the finalization.
        qDebug() << "defer recreation";
        QTimer::singleShot(100, this, SLOT(finishCanvasRecreation()));
#endif
    }
}*/

void Canvas::glActivate()
{
    makeCurrent();
}

void Canvas::glDeactivate()
{
    doneCurrent();
}

void *Canvas::nativeHandle() const
{
    return reinterpret_cast<void *>(winId());
}

void Canvas::exposeEvent(QExposeEvent *event)
{
    LOG_AS("Canvas");
    
    QOpenGLWindow::exposeEvent(event);

    // The first time the window is shown, run the initialization callback. On
    // some platforms, OpenGL is not fully ready to be used before the window
    // actually appears on screen.
    if(isExposed() && !isReady())
    {
        LOGDEV_GL_XVERBOSE("Window has been exposed, notifying about GL being ready");

#ifdef LIBGUI_USE_GLENTRYPOINTS
        glActivate();
        getAllOpenGLEntryPoints();
#endif
        GLInfo::glInit();
        d->makeReady();
    }
    else if(!isExposed() && isReady())
    {
        /// @todo Might be prudent to release (some) resources?
    }
}

bool Canvas::mainExists()
{
    return mainCanvas != 0;
}

Canvas &Canvas::main()
{
    DENG2_ASSERT(mainCanvas != 0);
    return *mainCanvas;
}

void Canvas::setMain(Canvas *canvas)
{
    mainCanvas = canvas;
}

} // namespace de
