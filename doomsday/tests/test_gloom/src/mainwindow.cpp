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
#include "gloomapp.h"
#include "approotwidget.h"
#include "globalshortcuts.h"

#include <de/CommandLine>
#include <de/GLState>
#include <de/Garbage>
#include <de/LabelWidget>
#include <de/LogBuffer>

using namespace de;

DENG2_PIMPL(MainWindow)
, DENG2_OBSERVES(GLWindow, Init)
, DENG2_OBSERVES(GLWindow, Resize)
{
    AppRootWidget    root;
    bool             needRootSizeUpdate{false};
    GlobalShortcuts *shortcuts{nullptr};
    LabelWidget *    test{nullptr};

    Impl(Public *i)
        : Base(i)
        , root(i)
    {
        self().audienceForInit()   += this;
        self().audienceForResize() += this;
    }

    ~Impl()
    {}

    void setupUI()
    {
        Style const &style = Style::get();

        shortcuts = new GlobalShortcuts;
        root.add(shortcuts);

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
        label->set(GuiWidget::Background(Vector4f(1.f, 1.f, 1.f, .5f)));
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
        label2->set(GuiWidget::Background(Vector4f(1.f, .5f, 0.f, .5f)));
        label2->rule()
                .setInput(Rule::Left,  label->rule().right())
                .setInput(Rule::Top,   label->rule().top())
                .setSize(label->rule().width(), label->rule().height());
        root.add(label2);
    }

    void windowInit(GLWindow &)
    {
        self().raise();
        self().requestActivate();
    }

    void updateRootSize()
    {
        DENG2_ASSERT_IN_MAIN_THREAD();

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
};

MainWindow::MainWindow(String const &id)
    : BaseWindow(id), d(new Impl(this))
{
    setTitle("test_gloom");
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
    GLState::current().target().clear(GLFramebuffer::ColorDepth);
    d->root.draw();
}

void MainWindow::preDraw()
{
    // NOTE: This occurs during the Canvas paintGL event.
    BaseWindow::preDraw();

    DENG2_ASSERT_IN_MAIN_THREAD();
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

bool MainWindow::handleFallbackEvent(Event const &)
{
    // Handle event at a global level, if appropriate.
    return false;
}
