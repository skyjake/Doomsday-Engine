/** @file glwindow_ios.h  Top-level OpenGL window (QML item).
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

#ifndef LIBGUI_GLWINDOW_IOS_H
#define LIBGUI_GLWINDOW_IOS_H

#if !defined (DE_MOBILE)
#  error "glwindow_qml.h is only for mobile platforms"
#endif

#include "../WindowEventHandler"
#include "../GLTextureFramebuffer"

#include <de/Vector>
#include <de/Rectangle>
#include <de/NativePath>

#include <QQuickItem>

namespace de {

/**
 * Top-level UI item that paints onto the OpenGL drawing surface. @ingroup gui
 */
class LIBGUI_PUBLIC GLWindow : public QObject, public Asset
{
    Q_OBJECT

public:
    typedef Vec2ui Size;

    /**
     * Notified when the canvas's GL state needs to be initialized. The OpenGL
     * context and drawing surface are not ready before this occurs. This gets
     * called immediately before drawing the contents of the WindowEventHandler for the
     * first time (during a paint event).
     */
    DE_DEFINE_AUDIENCE2(Init, void windowInit(GLWindow &))

    /**
     * Notified when a canvas's size has changed.
     */
    DE_DEFINE_AUDIENCE2(Resize, void windowResized(GLWindow &))

    /**
     * Notified when the contents of the canvas have been swapped to the window front
     * buffer and are thus visible to the user.
     */
    DE_DEFINE_AUDIENCE2(Swap, void windowSwapped(GLWindow &))

    DE_CAST_METHODS()

public:
    GLWindow();

    void setWindow(QQuickWindow *window);
    void setOpenGLContext(QOpenGLContext *context);

    void setTitle(QString const &title);
    QSurfaceFormat format() const;
    double devicePixelRatio() const;
    void makeCurrent();
    void doneCurrent();
    Rectanglei windowRect() const;
    Size fullscreenSize() const;
    void hide();
    void update();

    bool isGLReady() const;
    bool isFullScreen() const;
    bool isMaximized() const;
    bool isMinimized() const;
    bool isVisible() const;
    bool isHidden() const;

    float frameRate() const;
    uint frameCount() const;

    /**
     * Determines the current top left corner (origin) of the window.
     */
    inline Vec2i pos() const { return Vec2i(); }

    Size pointSize() const;
    Size pixelSize() const;

    int pointWidth() const;
    int pointHeight() const;
    int pixelWidth() const;
    int pixelHeight() const;

    int width() const = delete;
    int height() const = delete;

    /**
     * Returns a render target that renders to this canvas.
     *
     * @return GL render target.
     */
    GLFramebuffer &framebuffer() const;

    WindowEventHandler &eventHandler() const;

    /**
     * Determines if a WindowEventHandler instance is owned by this window.
     *
     * @param handler  WindowEventHandler instance.
     *
     * @return @c true or @c false.
     */
    bool ownsEventHandler(WindowEventHandler *handler) const;

    enum GrabMode
    {
        GrabNormal,
        GrabHalfSized
    };

    /**
     * Grabs the contents of the window and saves it into a native image file.
     *
     * @param path  Name of the file to save. May include a file extension
     *              that indicates which format to use (e.g, "screenshot.jpg").
     *              If omitted, defaults to PNG.
     *
     * @return @c true if successful, otherwise @c false.
     */
    bool grabToFile(NativePath const &path) const;

    /**
     * Grabs the contents of the canvas framebuffer.
     *
     * @param outputSize  If specified, the contents will be scaled to this size before
     *                    the image is returned.
     *
     * @return  Framebuffer contents (no alpha channel).
     */
    QImage grabImage(QSize const &outputSize = QSize()) const;

    /**
     * Grabs a portion of the contents of the canvas framebuffer.
     *
     * @param area        Portion to grab.
     * @param outputSize  If specified, the contents will be scaled to this size before
     *                    the image is returned.
     *
     * @return  Framebuffer contents (no alpha channel).
     */
    QImage grabImage(QRect const &area, QSize const &outputSize = QSize()) const;

    /**
     * Activates the window's GL context so that OpenGL API calls can be made.
     * The GL context is automatically active during the drawing of the window's
     * contents; at other times it needs to be manually activated.
     */
    void glActivate();

    /**
     * Dectivates the window's GL context after OpenGL API calls have been done.
     * The GL context is automatically deactived after the drawing of the window's
     * contents; at other times it needs to be manually deactivated.
     */
    void glDone();

    /**
     * Returns a handle to the native window instance. (Platform-specific.)
     */
    void *nativeHandle() const;

    void initializeGL();
    void resizeGL(int w, int h);

    virtual void draw() = 0;

public:
    static bool mainExists();
    static GLWindow &main();
    static void glActiveMain();
    static void setMain(GLWindow *window);

protected:
    virtual void windowAboutToClose();

    /*
    // Native events.
    void resizeEvent            (QResizeEvent *ev) override;
    void focusInEvent           (QFocusEvent  *ev) override;
    void focusOutEvent          (QFocusEvent  *ev) override;
    void keyPressEvent          (QKeyEvent    *ev) override;
    void keyReleaseEvent        (QKeyEvent    *ev) override;
    void mousePressEvent        (QMouseEvent  *ev) override;
    void mouseReleaseEvent      (QMouseEvent  *ev) override;
    void mouseDoubleClickEvent  (QMouseEvent  *ev) override;
    void mouseMoveEvent         (QMouseEvent  *ev) override;
    void wheelEvent             (QWheelEvent  *ev) override;

    bool event(QEvent *) override;
     */

signals:
    void textEntryRequest();
    void textEntryDismiss();
    void userEnteredText(QString);
    void userFinishedTextEntry();
    void rootDimensionsChanged(QRect);

public slots:
    void paintGL();
    void frameWasSwapped();

private:
    DE_PRIVATE(d)
};

/**
 * Performs OpenGL rendering primarily in the Qt render thread.
 */
class GLQuickItem : public QQuickItem
{
    Q_OBJECT

public:
    GLQuickItem();

    virtual GLWindow *makeWindowRenderer() = 0;

signals:
    void textEntryRequest();
    void textEntryDismiss();

public slots:
    void sync();
    void cleanup();

    void dimensionsChanged();
    void userEnteredText(QString text);
    void userFinishedTextEntry();

    void onTouchPressed(QVariantList touchPoints);
    void onTouchUpdated(QVariantList touchPoints);
    void onTouchReleased(QVariantList touchPoints);

private slots:
    void handleWindowChanged(QQuickWindow *win);

private:
    DE_PRIVATE(d)
};

/**
 * Template for instantiating a particular type of renderer in a GLQuickItem.
 */
template <typename RendererType>
class GLQuickItemT : public GLQuickItem
{
public:
    GLWindow *makeWindowRenderer() override
    {
        return new RendererType;
    }
};

} // namespace de

#endif // LIBGUI_GLWINDOW_IOS_H
