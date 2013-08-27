/** @file videosettingsdialog.cpp  Dialog for video settings.
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

#include "ui/widgets/videosettingsdialog.h"
#include "ui/widgets/variabletogglewidget.h"
#include "ui/widgets/choicewidget.h"
#include "ui/widgets/taskbarwidget.h"
#include "ui/widgets/sequentiallayout.h"
#include "ui/widgets/gridlayout.h"
#include "ui/clientwindow.h"
#include "ui/commandaction.h"
#include "ui/signalaction.h"
#include "con_main.h"

#include <de/App>
#include <de/DisplayMode>

using namespace de;
using namespace ui;

DENG2_PIMPL(VideoSettingsDialog),
DENG2_OBSERVES(PersistentCanvasWindow, AttributeChange)
{
    ClientWindow &win;
    VariableToggleWidget *showFps;
    ToggleWidget *fullscreen;
    ToggleWidget *maximized;
    ToggleWidget *centered;
    ToggleWidget *fsaa;
    ToggleWidget *vsync;
    ChoiceWidget *modes;
    ChoiceWidget *depths;

    Instance(Public *i) : Base(i), win(ClientWindow::main())
    {
        ScrollAreaWidget &area = self.area();
        area.add(showFps    = new VariableToggleWidget(App::config()["window.main.showFps"]));
        area.add(fullscreen = new ToggleWidget);
        area.add(maximized  = new ToggleWidget);
        area.add(centered   = new ToggleWidget);
        area.add(fsaa       = new ToggleWidget);
        area.add(vsync      = new ToggleWidget);
        area.add(modes      = new ChoiceWidget);
        area.add(depths     = new ChoiceWidget);

        win.audienceForAttributeChange += this;
    }

    ~Instance()
    {
        win.audienceForAttributeChange -= this;
    }

    /**
     * Updates the widgets with the actual current state.
     */
    void fetch()
    {
        fullscreen->setActive(win.isFullScreen());
        maximized->setActive(win.isMaximized());
        centered->setActive(win.isCentered());
        fsaa->setActive(Con_GetInteger("vid-fsaa") != 0);
        vsync->setActive(Con_GetInteger("vid-vsync") != 0);
    }

    void windowAttributesChanged(PersistentCanvasWindow &)
    {
        fetch();
    }
};

VideoSettingsDialog::VideoSettingsDialog(String const &name)
    : DialogWidget(name), d(new Instance(this))
{
    // Fullscreen, Maximized, Centered, FPS, FSAA, VSync

    // Toggles for video/window options.
    d->fullscreen->setText(tr("Fullscreen"));
    d->fullscreen->setAction(new CommandAction("togglefullscreen"));

    d->maximized->setText(tr("Maximized"));
    d->maximized->setAction(new CommandAction("togglemaximized"));

    d->centered->setText(tr("Center Window"));
    d->centered->setAction(new CommandAction("togglecentered"));

    d->showFps->setText(tr("Show FPS"));

    d->fsaa->setText(tr("Antialias"));
    d->fsaa->setAction(new SignalAction(this, SLOT(toggleAntialias())));

    d->vsync->setText(tr("VSync"));
    d->vsync->setAction(new SignalAction(this, SLOT(toggleVerticalSync())));

    LabelWidget *modeLabel = new LabelWidget;
    modeLabel->setText(tr("Mode:"));
    area().add(modeLabel);

    // Choice of display modes + 16/32-bit color depth.
    d->modes->setOpeningDirection(ui::Up);
    for(int i = 0; i < DisplayMode_Count(); ++i)
    {
        DisplayMode const *m = DisplayMode_ByIndex(i);
        String desc = String("%1 x %2 (%3:%4)").arg(m->width).arg(m->height)
                .arg(m->ratioX).arg(m->ratioY);
        if(m->refreshRate > 0) desc += String(" @ %1 Hz").arg(m->refreshRate, 0, 'f', 1);

        d->modes->items() << new ChoiceItem(desc, i);
    }

    LabelWidget *colorLabel = new LabelWidget;
    colorLabel->setText(tr("Colors:"));
    area().add(colorLabel);

    d->depths->setOpeningDirection(ui::Up);
    d->depths->items()
            << new ChoiceItem(tr("32-bit"), 32)
            << new ChoiceItem(tr("16-bit"), 16);

    buttons().items()
            << new DialogButtonItem(DialogWidget::Action, tr("Reset to Defaults"))
            << new DialogButtonItem(DialogWidget::Action, tr("Color Adjustments..."),
                                    new SignalAction(&d->win.taskBar(), SLOT(closeMainMenu())));

    // Layout all widgets.
    GridLayout layout(area().contentRule().left(),
                      area().contentRule().top(), GridLayout::RowFirst);
    layout.setGridSize(2, 3);
    layout.setColumnPadding(style().rules().rule("gap"));
    layout << *d->showFps
           << *d->fsaa
           << *d->vsync
           << *d->fullscreen
           << *d->maximized
           << *d->centered;

    SequentialLayout modeLayout(d->vsync->rule().left(), d->vsync->rule().bottom(), ui::Right);
    modeLayout << *modeLabel << *d->modes << *colorLabel << *d->depths;

    /*SequentialLayout layout2(modeLabel->rule().left(), modeLabel->rule().bottom());
    layout2 << *color << *def;*/

    area().setContentSize(OperatorRule::maximum(layout.width(),
                                                modeLayout.width()
                                                /*layout2.width()*/),
                          layout.height() + modeLayout.height()/* + layout2.height()*/);

    d->fetch();
}

void VideoSettingsDialog::toggleAntialias()
{
    Con_SetInteger("vid-fsaa", !Con_GetInteger("vid-fsaa"));
}

void VideoSettingsDialog::toggleVerticalSync()
{
    Con_SetInteger("vid-vsync", !Con_GetInteger("vid-vsync"));
}
