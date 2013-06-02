/** @file taskbarwidget.cpp
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/widgets/taskbarwidget.h"
#include "ui/widgets/guirootwidget.h"
#include "ui/widgets/labelwidget.h"
#include "ui/widgets/buttonwidget.h"
#include "ui/commandaction.h"

#include <de/KeyEvent>
#include <de/Drawable>
#include <de/GLBuffer>
#include <de/ScalarRule>

#include "../../updater/versioninfo.h"
#include "dd_main.h"

using namespace de;

DENG2_PIMPL(TaskBarWidget),
DENG2_OBSERVES(Games, GameChange)
{
    typedef DefaultVertexBuf VertexBuf;

    bool opened;
    ButtonWidget *logo;
    LabelWidget *status;
    ScalarRule *vertShift;

    QScopedPointer<Action> openAction;
    QScopedPointer<Action> closeAction;

    // GL objects:
    Drawable drawable;
    GLUniform uMvpMatrix;
    GLUniform uColor;
    Matrix4f projMatrix;

    Instance(Public *i)
        : Base(i),
          opened(true),
          status(0),
          uMvpMatrix("uMvpMatrix", GLUniform::Mat4),
          uColor    ("uColor",     GLUniform::Vec4)
    {
        uColor = Vector4f(1, 1, 1, 1);
        self.set(Background(self.style().colors().colorf("background")));

        vertShift = new ScalarRule(0);

        App_Games().audienceForGameChange += this;
    }

    ~Instance()
    {
        App_Games().audienceForGameChange -= this;

        releaseRef(vertShift);
    }

    void glInit()
    {
        drawable.addBuffer(new VertexBuf);

        self.root().shaders().build(drawable.program(), "generic.color_ucolor")
                << uMvpMatrix
                << uColor;

        updateProjection();
    }

    void glDeinit()
    {
        drawable.clear();
    }

    void updateGeometry()
    {
        Rectanglei pos;
        if(self.hasChangedPlace(pos) || self.geometryRequested())
        {
            self.requestGeometry(false);

            VertexBuf::Builder verts;
            self.glMakeGeometry(verts);
            drawable.buffer<VertexBuf>().setVertices(gl::TriangleStrip, verts, gl::Static);
        }
    }

    void updateProjection()
    {
        uMvpMatrix = self.root().projMatrix2D();
    }

    void currentGameChanged(Game &)
    {
        updateStatus();
    }

    void updateStatus()
    {
        if(App_GameLoaded())
        {
            status->setText(Str_Text(App_Games().current().identityKey()));
        }
        else
        {
            status->setText("No game loaded");
        }
    }
};

TaskBarWidget::TaskBarWidget() : GuiWidget("TaskBar"), d(new Instance(this))
{
    Rule const &gap = style().rules().rule("gap");

    Background bg(style().colors().colorf("background"));

    d->logo = new ButtonWidget;
    d->logo->setAction(new CommandAction("panel"));
    d->logo->setImage(style().images().image("logo.px128"));
    d->logo->setImageScale(.6f);
    d->logo->setImageFit(FitToHeight | OriginalAspectRatio);
    d->logo->setText(DENG2_ESC("b") + VersionInfo().base());
    d->logo->setWidthPolicy(LabelWidget::Expand);
    d->logo->setTextAlignment(AlignLeft);
    d->logo->rule()
            .setInput(Rule::Height, rule().height())
            .setInput(Rule::Right,  rule().right())
            .setInput(Rule::Bottom, rule().bottom());
    add(d->logo);

    // Currently loaded game.
    d->status = new LabelWidget;
    d->status->set(bg);
    d->status->setWidthPolicy(LabelWidget::Expand);
    d->status->rule()
            .setInput(Rule::Height, rule().height())
            .setInput(Rule::Bottom, rule().bottom())
            .setInput(Rule::Right,  d->logo->rule().left());
    add(d->status);        

    d->updateStatus();

    ButtonWidget *console = new ButtonWidget;
    console->setText("Console");
    console->setWidthPolicy(LabelWidget::Expand);
    console->setAction(new CommandAction("contoggle"));
    console->rule()
            .setInput(Rule::Height, rule().height())
            .setInput(Rule::Left,   rule().left())
            .setInput(Rule::Bottom, rule().bottom());
    add(console);

    /*
    ButtonWidget *panel = new ButtonWidget;
    panel->setImage(style().images().image("gear"));
    panel->setWidthPolicy(LabelWidget::Expand);
    panel->setHeightPolicy(LabelWidget::Filled);
    panel->setImageFit(FitToHeight | OriginalAspectRatio);
    panel->setAction(new CommandAction("panel"));
    panel->rule()
            .setInput(Rule::Height, rule().height())
            .setInput(Rule::Left,   console->rule().right())
            .setInput(Rule::Bottom, rule().bottom());
    add(panel);*/

    // Taskbar height depends on the font size.
    rule().setInput(Rule::Height, style().fonts().font("default").height() + gap * 2);
}

bool TaskBarWidget::isOpen() const
{
    return d->opened;
}

Rule const &TaskBarWidget::shift()
{
    return *d->vertShift;
}

void TaskBarWidget::setOpeningAction(Action *action)
{
    d->openAction.reset(action);
}

void TaskBarWidget::setClosingAction(Action *action)
{
    d->closeAction.reset(action);
}

void TaskBarWidget::glInit()
{
    LOG_AS("TaskBarWidget");
    d->glInit();
}

void TaskBarWidget::glDeinit()
{
    d->glDeinit();
}

void TaskBarWidget::viewResized()
{
    d->updateProjection();
}

void TaskBarWidget::draw()
{
    d->updateGeometry();
    //d->drawable.draw();
}

bool TaskBarWidget::handleEvent(Event const &event)
{
    if(event.type() == Event::KeyPress)
    {
        KeyEvent const &key = event.as<KeyEvent>();
        if(key.qtKey() == Qt::Key_Escape)
        {
            if(isOpen())
                close();
            else
                open();

            if(!key.modifiers().testFlag(KeyEvent::Shift))
                return true;
        }
    }
    return false;
}

void TaskBarWidget::open()
{
    if(!d->opened)
    {
        d->opened = true;
        d->vertShift->set(0, .2f);
        d->logo->setOpacity(1, .2f);
        d->status->setOpacity(1, .2f);

        emit opened();

        if(!d->openAction.isNull())
        {
            d->openAction->trigger();
        }
    }
}

void TaskBarWidget::close()
{
    if(d->opened)
    {
        d->opened = false;
        d->vertShift->set(rule().height().valuei() + style().rules().rule("unit").valuei(), .2f);
        d->logo->setOpacity(0, .2f);
        d->status->setOpacity(0, .2f);

        emit closed();

        if(!d->closeAction.isNull())
        {
            d->closeAction->trigger();
        }
    }
}

