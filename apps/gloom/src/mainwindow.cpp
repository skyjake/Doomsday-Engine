/** @file mainwindow.cpp  The main window.
 *
 * @authors Copyright (c) 2014-2018 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "mainwindow.h"
#include "gloomapp.h"
#include "approotwidget.h"
#include "globalshortcuts.h"
#include "gloomwidget.h"

#include <de/commandline.h>
#include <de/glstate.h>
#include <de/garbage.h>
#include <de/labelwidget.h>
#include <de/logbuffer.h>

using namespace de;
using namespace gloom;

DE_PIMPL(MainWindow)
, DE_OBSERVES(GLWindow, Init)
, DE_OBSERVES(GLWindow, Resize)
, DE_OBSERVES(WindowEventHandler, FocusChange)
{
    AppRootWidget    root;
    bool             needRootSizeUpdate{false};
    GlobalShortcuts *shortcuts{nullptr};
    LabelWidget *    info{nullptr};
    GloomWidget *    gloom{nullptr};

    Impl(Public *i)
        : Base(i)
        , root(i)
    {
        self().audienceForInit()   += this;
        self().audienceForResize() += this;
        self().eventHandler().audienceForFocusChange() += this;
    }

    void setupUI()
    {
        //const Style &style = Style::get();

        shortcuts = new GlobalShortcuts;
        root.add(shortcuts);

        gloom = new GloomWidget;
        gloom->rule().setRect(root.viewRule());
        root.add(gloom);

        info = new LabelWidget;
        info->setText(_E(b) "Gloom");
        info->setSizePolicy(ui::Expand, ui::Expand);
        info->rule().setLeftTop(root.viewLeft(), root.viewTop());
        root.add(info);

        /*
        test = new LabelWidget;
        test->setText("Gloom");
        test->setImage(GloomApp::images().image("logo"));
        test->setTextAlignment(ui::AlignBottom);
        test->rule().setRect(root.viewRule());
        root.add(test);

        // Try different label parameters.
        LabelWidget *label = new LabelWidget;
        label->setImage(GloomApp::images().image("logo"));
        label->setSizePolicy(ui::Filled, ui::Filled);
        label->setImageFit(ui::OriginalAspectRatio | ui::FitToHeight | ui::FitToSize);
        label->set(GuiWidget::Background(Vec4f(1.f, 1.f, 1.f, .5f)));
        label->rule()
                .setInput(Rule::Right,  root.viewRule().midX())
                .setInput(Rule::Top,    root.viewRule().top())
                .setInput(Rule::Width,  root.viewRule().width()/3)
                .setInput(Rule::Height, Const(label->toDevicePixels(300)));
        root.add(label);

        LabelWidget *label2 = new LabelWidget;
        label2->setImage(GloomApp::images().image("logo"));
        label2->setSizePolicy(ui::Filled, ui::Filled);
        label2->setImageFit(ui::OriginalAspectRatio | ui::FitToHeight);
        label2->set(GuiWidget::Background(Vec4f(1.f, .5f, 0.f, .5f)));
        label2->rule()
                .setInput(Rule::Left,  label->rule().right())
                .setInput(Rule::Top,   label->rule().top())
                .setSize(label->rule().width(), label->rule().height());
        root.add(label2);
        */
    }

    void windowInit(GLWindow &)
    {
        self().raise();
        //self().requestActivate();
    }

    void updateRootSize()
    {
        DE_ASSERT_IN_MAIN_THREAD();

        needRootSizeUpdate = false;

        // Tell the widgets.
        root.setViewSize(self().pixelSize());
    }

    void windowResized(GLWindow &)
    {
        LOG_AS("MainWindow");

        Size size = self().pixelSize();
        LOG_TRACE("Window resized to %s pixels", size.asText());

        // Update viewport.
        GLState::current().setViewport(Rectangleui(0, 0, size.x, size.y));

        updateRootSize();
    }

    void windowFocusChanged(GLWindow &, bool /*hasFocus*/)
    {
//        auto &loop = Loop::get();
//        loop.setRate(hasFocus ? 60 : 2);
    }
};

MainWindow::MainWindow(const String &id)
    : BaseWindow(id)
    , d(new Impl(this))
{
    setTitle("Gloom");
    d->setupUI();
}

AppRootWidget &MainWindow::root()
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
    d->root.draw();
}

void MainWindow::preDraw()
{
    // NOTE: This occurs during the Canvas paintGL event.
    BaseWindow::preDraw();

    DE_ASSERT_IN_MAIN_THREAD();
    if (d->needRootSizeUpdate)
    {
        d->updateRootSize();
    }
}

void MainWindow::postDraw()
{
    BaseWindow::postDraw();
    Garbage_Recycle();
}

//bool MainWindow::handleFallbackEvent(const Event &)
//{
//    // Handle event at a global level, if appropriate.
//    return false;
//}
