/** @file mainwindow.cpp  The main window.
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "mainwindow.h"
#include "testapp.h"
#include "approotwidget.h"
#include "globalshortcuts.h"

#include <de/Garbage>
#include <de/GLState>
#include <de/VRConfig>
#include <de/VRWindowTransform>
#include <de/CompositorWidget>
#include <de/LabelWidget>

using namespace de;

DENG2_PIMPL(MainWindow)
, DENG2_OBSERVES(Canvas, GLResize)
{
    AppRootWidget root;
    bool needRootSizeUpdate;
    VRWindowTransform contentXf;

    GlobalShortcuts *shortcuts;
    CompositorWidget *compositor;
    LabelWidget *test;

    // Faux mouse cursor for transformed VR mode.
    LabelWidget *cursor;
    LabelWidget *camPos;
    ConstantRule *cursorX;
    ConstantRule *cursorY;

    Instance(Public *i)
        : Base(i)
        , root(&self)
        , needRootSizeUpdate(false)
        , contentXf(self)
        , compositor(0)
        , test(0)
        , cursor(0)
        , cursorX(new ConstantRule(0))
        , cursorY(new ConstantRule(0))
    {
        self.setTransform(contentXf);
        self.canvas().audienceForGLResize() += this;
    }

    ~Instance()
    {
        TestApp::vr().oculusRift().deinit();

        releaseRef(cursorX);
        releaseRef(cursorY);
    }

    void setupUI()
    {
        Style const &style = Style::get();

        shortcuts = new GlobalShortcuts;
        root.add(shortcuts);

        compositor = new CompositorWidget;
        root.add(compositor);

        test = new LabelWidget;
        test->setText("Doomsday Application Framework Test");
        test->setImage(TestApp::images().image("logo"));
        test->setTextAlignment(ui::AlignBottom);
        test->rule().setRect(root.viewRule());
        compositor->add(test);

        // Mouse cursor.
        cursor = new LabelWidget;
        cursor->setBehavior(Widget::Unhittable);
        cursor->margins().set(""); // no margins
        cursor->setImage(style.images().image("window.cursor"));
        cursor->setAlignment(ui::AlignTopLeft);
        cursor->rule()
                .setSize(Const(48), Const(48))
                .setLeftTop(*cursorX, *cursorY);
        compositor->add(cursor);

        // The mouse cursor is only needed with OVR.
        if(!VRConfig::modeAppliesDisplacement(TestApp::vr().mode()))
        {
            cursor->hide();
        }
    }

    void glInit()
    {
        GLState::current()
                .setBlend(true)
                .setBlendFunc(gl::SrcAlpha, gl::OneMinusSrcAlpha);

        contentXf.glInit();

        if(TestApp::vr().mode() == VRConfig::OculusRift)
        {            
            TestApp::vr().oculusRift().init();
        }

        self.raise();
        self.activateWindow();
        self.canvas().setFocus();
    }

    void updateMouseCursor()
    {
        Vector2i cp = TestApp::windowSystem().latestMousePosition();
        if(cp.x < 0 || cp.y < 0) return;
        cursorX->set(cp.x);
        cursorY->set(cp.y);
    }

    void updateRootSize()
    {
        DENG2_ASSERT_IN_MAIN_THREAD();

        needRootSizeUpdate = false;

        Vector2ui const size = contentXf.logicalRootSize(self.canvas().size());

        // Tell the widgets.
        root.setViewSize(size);
    }

    void updateCompositor()
    {
        DENG2_ASSERT_IN_MAIN_THREAD();
        if(!compositor) return;

        VRConfig &vr = TestApp::vr();

        if(vr.mode() == VRConfig::OculusRift)
        {
            compositor->setCompositeProjection(
                        vr.projectionMatrix(OVR_FOV, root.viewRule().size(),
                                            OVR_NEAR_CLIP, OVR_FAR_CLIP)
                        * Matrix4f::scale(Vector3f(1, -1 / vr.oculusRift().aspect(), 1))
                        * Matrix4f::translate(Vector3f(-.5f, -.5f, -1)));
        }
        else
        {
            // We'll simply cover the entire view.
            compositor->useDefaultCompositeProjection();
        }
    }

    void canvasGLResized(Canvas &canvas)
    {
        LOG_AS("MainWindow");

        Canvas::Size size = canvas.size();
        LOG_TRACE("Canvas resized to ") << size.asText();

        // Update viewport.
        GLState::current().setViewport(Rectangleui(0, 0, size.x, size.y));

        updateRootSize();
    }
};

MainWindow::MainWindow(String const &id)
    : BaseWindow(id), d(new Instance(this))
{
    if(App::commandLine().has("--ovr"))
    {
        // Go straight into Oculus Rift mode.
        VRConfig &vr = TestApp::vr();
        vr.setMode(VRConfig::OculusRift);
        vr.setRiftFramebufferSampleCount(App::commandLine().has("--nofsaa")? 1 : 2);
        vr.setPhysicalPlayerHeight(1.8f);
        vr.setScreenDistance(.5f);
        vr.setEyeHeightInMapUnits(vr.physicalPlayerHeight() * .925f);
        setCursor(Qt::BlankCursor);
    }

    setWindowTitle("test_appfw");

    d->setupUI();
}

AppRootWidget &MainWindow::root()
{
    return d->root;
}

Vector2f MainWindow::windowContentSize() const
{
    // Current root widget size.
    return d->root.viewRule().size();
}

void MainWindow::drawWindowContent()
{
    GLState::current().target().clear(GLTarget::ColorDepth);

    d->updateCompositor();
    d->root.draw();
}

void MainWindow::canvasGLReady(Canvas &canvas)
{
    BaseWindow::canvasGLReady(canvas);

    // Configure a viewport immediately.
    GLState::current()
            .setViewport(Rectangleui(0, 0, canvas.width(), canvas.height()))
            .setDepthTest(true);

    LOGDEV_MSG("MainWindow GL ready");

    d->glInit();
}

void MainWindow::preDraw()
{
    // NOTE: This occurs during the Canvas paintGL event.
    BaseWindow::preDraw();

    DENG2_ASSERT_IN_MAIN_THREAD();

    d->updateMouseCursor();
    if(d->needRootSizeUpdate)
    {
        d->updateRootSize();
    }
}

void MainWindow::postDraw()
{
    if(TestApp::vr().mode() != VRConfig::OculusRift)
    {
        swapBuffers();
    }
    BaseWindow::postDraw();

    Garbage_Recycle();
}

void MainWindow::addOnTop(de::GuiWidget *widget)
{
    d->compositor->add(widget);

    // Keep the mouse cursor on top.
    d->compositor->moveChildToLast(*d->cursor);
}

bool MainWindow::handleFallbackEvent(Event const &)
{
    // Handle event at a global level, if appropriate.
    return false;
}
