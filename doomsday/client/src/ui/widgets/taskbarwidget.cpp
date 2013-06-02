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

#include <de/Drawable>
#include <de/GLBuffer>

#include "../../updater/versioninfo.h"
#include "dd_main.h"

using namespace de;

DENG2_PIMPL(TaskBarWidget),
DENG2_OBSERVES(Games, GameChange)
{
    typedef DefaultVertexBuf VertexBuf;

    ConsoleCommandWidget *cmdLine;
    LabelWidget *status;

    Drawable drawable;
    GLUniform uMvpMatrix;
    GLUniform uColor;
    Matrix4f projMatrix;

    Instance(Public *i)
        : Base(i),
          cmdLine(0),
          status(0),
          uMvpMatrix("uMvpMatrix", GLUniform::Mat4),
          uColor    ("uColor",     GLUniform::Vec4)
    {
        uColor = Vector4f(1, 1, 1, 1);
        self.set(Background(self.style().colors().colorf("background")));

        App_Games().audienceForGameChange += this;
    }

    ~Instance()
    {
        App_Games().audienceForGameChange -= this;
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

    LabelWidget *logo = new LabelWidget;
    logo->setImage(style().images().image("logo.px128"));
    logo->setImageScale(.6f);
    logo->setImageFit(FitToHeight | OriginalAspectRatio);
    logo->setText(DENG2_ESC("b") + VersionInfo().base());
    logo->setWidthPolicy(LabelWidget::Expand);
    logo->setTextAlignment(AlignLeft);
    logo->rule()
            .setInput(Rule::Height, rule().height())
            .setInput(Rule::Right,  rule().right())
            .setInput(Rule::Bottom, rule().bottom());
    add(logo);

    // Currently loaded game.
    d->status = new LabelWidget;
    d->status->setWidthPolicy(LabelWidget::Expand);
    d->status->rule()
            .setInput(Rule::Height, rule().height())
            .setInput(Rule::Bottom, rule().bottom())
            .setInput(Rule::Right,  logo->rule().left());
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

    ButtonWidget *panel = new ButtonWidget;
    panel->setText("Settings");
    panel->setWidthPolicy(LabelWidget::Expand);
    panel->setAction(new CommandAction("panel"));
    panel->rule()
            .setInput(Rule::Height, rule().height())
            .setInput(Rule::Left,   console->rule().right())
            .setInput(Rule::Bottom, rule().bottom());

    add(panel);
    // Taskbar height depends on the font size.
    rule().setInput(Rule::Height, style().fonts().font("default").height() + gap * 2);
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
    d->drawable.draw();
}
