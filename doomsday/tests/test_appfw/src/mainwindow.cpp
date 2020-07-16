/** @file mainwindow.cpp  The main window.
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <de/buttonwidget.h>
#include <de/commandline.h>
#include <de/compositorwidget.h>
#include <de/flowlayout.h>
#include <de/glstate.h>
#include <de/garbage.h>
#include <de/labelwidget.h>
#include <de/logbuffer.h>
#include <de/vrconfig.h>
#include <de/vrwindowtransform.h>

using namespace de;

DE_PIMPL(MainWindow)
, DE_OBSERVES(GLWindow, Init)
, DE_OBSERVES(GLWindow, Resize)
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

    Impl(Public *i)
        : Base(i)
        , root(i)
        , needRootSizeUpdate(false)
        , contentXf(*i)
        , compositor(nullptr)
        , test(nullptr)
        , cursor(nullptr)
        , cursorX(new ConstantRule(0))
        , cursorY(new ConstantRule(0))
    {
        self().setTransform(contentXf);
        self().audienceForInit()   += this;
        self().audienceForResize() += this;
    }

    ~Impl()
    {
        releaseRef(cursorX);
        releaseRef(cursorY);
    }

    void setupUI()
    {
        const auto &style = Style::get();

        shortcuts = new GlobalShortcuts;
        root.add(shortcuts);

        compositor = new CompositorWidget;
        root.add(compositor);

        test = new LabelWidget;
        test->setText(_E(b) "Doomsday" _E(.)" Application" _E(i)" Framework " _E(.)"Test");
        test->setImage(TestApp::images().image("logo"));
        test->setTextAlignment(ui::AlignBottom);
        test->rule().setRect(root.viewRule());
        compositor->add(test);

        // Try different label parameters.
        LabelWidget *label = new LabelWidget;
        label->setImage(TestApp::images().image("logo"));
        label->setSizePolicy(ui::Filled, ui::Filled);
        label->setImageFit(ui::OriginalAspectRatio | ui::FitToHeight | ui::FitToSize);
        label->set(GuiWidget::Background(Vec4f(1.f, 1.f, 1.f, .5f)));
        label->rule()
                .setInput(Rule::Right,  root.viewRule().midX())
                .setInput(Rule::Top,    root.viewRule().top())
                .setInput(Rule::Width,  root.viewRule().width()/3)
                .setInput(Rule::Height, Const(GuiWidget::pointsToPixels(300)));
        compositor->add(label);

        LabelWidget *label2 = new LabelWidget;
        label2->setImage(TestApp::images().image("logo"));
        label2->setSizePolicy(ui::Filled, ui::Filled);
        label2->setImageFit(ui::OriginalAspectRatio | ui::FitToHeight);
        label2->set(GuiWidget::Background(Vec4f(1.f, .5f, 0.f, .5f)));
        label2->rule()
                .setInput(Rule::Left,  label->rule().right())
                .setInput(Rule::Top,   label->rule().top())
                .setSize(label->rule().width(), label->rule().height());
        compositor->add(label2);

        ButtonWidget *button = new ButtonWidget;
        button->setSizePolicy(ui::Expand, ui::Expand);
        button->setText("Press Me");
        button->rule()
                .setMidAnchorX(test->rule().midX())
                .setInput(Rule::Bottom, test->rule().bottom());
        button->audienceForPress() += []() {
            TestApp::app().createAnotherWindow();
        };
        compositor->add(button);

        // Try some flow.
#if defined (DE_DEBUG)
        const int count1 = Counted::totalCount;
#endif
        {
            FlowLayout flow(Const(0), Const(0), root.viewWidth() / 2);

            for (int i = 0; i < 26; ++i)
            {
                auto *lw = new LabelWidget(i == 25 ? "test-letter" : "");
                lw->setText(Stringf("%c", 'A' + i));
                lw->set(GuiWidget::Background(Vec4f(randf(), randf(), randf(), 0.25f)));
                lw->setSizePolicy(ui::Expand, ui::Expand);
                lw->margins().set("gap");

                flow << *lw;
                compositor->add(lw);
            }
        }
#if defined (DE_DEBUG)
        const int count2 = Counted::totalCount;
        debug("de::Counted increase during FlowLayout creation: %i", count2 - count1);
#endif

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
        if (!VRConfig::modeAppliesDisplacement(TestApp::vr().mode()))
        {
            cursor->hide();
        }
    }

    void windowInit(GLWindow &)
    {
        contentXf.glInit();

        self().raise();
    }

    void updateMouseCursor()
    {
        const Vec2i cp = self().latestMousePosition();
        if (cp.x < 0 || cp.y < 0) return;
        cursorX->set(cp.x);
        cursorY->set(cp.y);
    }

    void updateRootSize()
    {
        DE_ASSERT_IN_MAIN_THREAD();

        needRootSizeUpdate = false;

        const Vec2ui size = contentXf.logicalRootSize(self().pixelSize());

        // Tell the widgets.
        root.setViewSize(size);
    }

    void updateCompositor()
    {
        DE_ASSERT_IN_MAIN_THREAD();
        if (!compositor) return;

        VRConfig &vr = TestApp::vr();

        if (vr.mode() == VRConfig::OculusRift)
        {
            compositor->setCompositeProjection(
                        vr.projectionMatrix(OVR_FOV, root.viewRule().size(),
                                            OVR_NEAR_CLIP, OVR_FAR_CLIP)
                        * Mat4f::scale(Vec3f(.5f, -.5f / vr.oculusRift().aspect(), 1))
                        * Mat4f::translate(Vec3f(-.5f, -.5f, -1)));
        }
        else
        {
            // We'll simply cover the entire view.
            compositor->useDefaultCompositeProjection();
        }
    }

    void windowResized(GLWindow &)
    {
        LOG_AS("MainWindow");

        Size size = self().pixelSize();
        LOG_TRACE("Window resized to %s pixels", size.asText());

        // Update viewport.
        GLState::current().setViewport(Rectangleui(0, 0, size.x, size.y));

        updateRootSize();

        if (const auto *letter = root.guiFind("test-letter"))
        {
            debug(letter->rule().description().c_str());
        }
    }
};

MainWindow::MainWindow(const String &id)
    : BaseWindow(id), d(new Impl(this))
{
#if 0
    if (App::commandLine().has("--ovr"))
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
#endif

    setTitle("test_appfw");

    d->setupUI();
}

GuiRootWidget &MainWindow::root()
{
    return d->root;
}

Vec2f MainWindow::windowContentSize() const
{
    // Current root widget size.
    return d->root.viewRule().size();
}

void MainWindow::drawWindowContent()
{
    GLState::current().target().clear(GLFramebuffer::ColorDepth);

    d->updateCompositor();
    d->root.draw();
}

void MainWindow::preDraw()
{
    // NOTE: This occurs during the Canvas paintGL event.
    BaseWindow::preDraw();

    DE_ASSERT_IN_MAIN_THREAD();

    d->updateMouseCursor();
    if (d->needRootSizeUpdate)
    {
        d->updateRootSize();
    }
}

void MainWindow::postDraw()
{
    /*if (TestApp::vr().mode() != VRConfig::OculusRift)
    {
        swapBuffers();
    }*/
    BaseWindow::postDraw();

    Garbage_Recycle();
}

void MainWindow::windowAboutToClose()
{
    BaseWindow::windowAboutToClose();
    close();
}

void MainWindow::addOnTop(de::GuiWidget *widget)
{
    d->compositor->add(widget);

    // Keep the mouse cursor on top.
    d->compositor->moveChildToLast(*d->cursor);
}

//bool MainWindow::handleFallbackEvent(const Event &)
//{
//    // Handle event at a global level, if appropriate.
//    return false;
//}
